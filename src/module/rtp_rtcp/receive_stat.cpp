//
// Created by faker on 24-2-3.
//

#include "receive_stat.h"
#include "rtc_base/logging.h"

namespace xrtc{
    const int kMaxReorderingThreshold = 450;
    const int kStreamStatTimeoutMs = 8000;

    ReceiveStat::ReceiveStat(webrtc::Clock *clock): clock_(clock) {

    }

    ReceiveStat::~ReceiveStat() {

    }

    void ReceiveStat::OnRtpPacket(const webrtc::RtpPacketReceived &packet) {

//        RTC_LOG(LS_INFO) << "ReceiveStat::OnRtpPacket";
        GetOrCreateStat(packet.Ssrc())->UpdateCounters(packet);
    }

    std::unique_ptr<ReceiveStat> ReceiveStat::Create(webrtc::Clock *clock) {
        return std::make_unique<ReceiveStat>(clock);
    }

    StreamStat *ReceiveStat::GetOrCreateStat(uint32_t ssrc) {
        // 如果这个map没有 就插入一个值
        std::unique_ptr<StreamStat>& stat = stats_[ssrc];
        if(nullptr == stat){
            stat = std::make_unique<StreamStat>(ssrc, clock_);
            all_ssrcs_.push_back(ssrc);
        }
        return stat.get();
    }

    std::vector<webrtc::rtcp::ReportBlock> ReceiveStat::RtcpReportBlocks(size_t max_blocks) {
        std::vector<webrtc::rtcp::ReportBlock> result;
        result.reserve(std::min(max_blocks,all_ssrcs_.size()));
        size_t ssrc_idx = 0;
        for(size_t i = 0; i < all_ssrcs_.size() && result.size() < all_ssrcs_.size(); ++i){
            ssrc_idx = (last_returned_ssrc_idx_ + 1 + i) % all_ssrcs_.size();
            uint32_t media_ssrc = all_ssrcs_[ssrc_idx];
            auto stat_it = stats_.find(media_ssrc);
            stat_it->second->MaybeAppendReportBlockAndReset(result);
        }
        return result;
    }

    StreamStat::StreamStat(uint32_t ssrc, webrtc::Clock *clock): ssrc_(ssrc), clock_(clock), max_reordering_threshold_(kMaxReorderingThreshold){

    }

    StreamStat::~StreamStat() {

    }

    void StreamStat::UpdateCounters(const webrtc::RtpPacketReceived &packet) {
        int64_t now_ms = clock_->TimeInMilliseconds();

        receive_counters_.transmitted.AddPacket(packet);
        --cumulative_loss_;

        int64_t sequence_number = seq_unwrapper_.UnwrapWithoutUpdate(packet.SequenceNumber());
        // 收到第一个包
        if (!ReceiveRtpPacket()){
            received_seq_first_ = sequence_number;
            received_seq_max_ = sequence_number - 1;
        }else if(UpdateOutOfOrder(packet,sequence_number,now_ms)){
            return;
        }
        // 顺序到达rtp
        cumulative_loss_ += (sequence_number - received_seq_max_);
        received_seq_max_ = sequence_number;
        seq_unwrapper_.UpdateLast(sequence_number);
        // 计算jitter
        if(packet.Timestamp() != last_received_timestamp_ && (receive_counters_.transmitted.packets > - receive_counters_.retransmitted.packets) > 1){
            UpdateJitter(packet, now_ms);
        }
        last_received_timestamp_ = packet.Timestamp();
        last_received_time_ms_ = now_ms;
    }
    bool StreamStat::UpdateOutOfOrder(const webrtc::RtpPacketReceived& packet,
                                      int64_t sequence_number,
                                      int64_t now_ms) {
        if(received_seq_out_of_order_){
            uint16_t expected_seq_num = *received_seq_out_of_order_ + 1;
            received_seq_out_of_order_ = absl::nullopt;
            // 认定发生序列号突变，重置计数状态
            if(packet.SequenceNumber() == expected_seq_num){
                received_seq_max_ = sequence_number - 2;
                return false;
            }
        }
        // 有可能流序列号发生突变
        if(abs(sequence_number -  received_seq_max_) > max_reordering_threshold_){
            received_seq_out_of_order_ = packet.SequenceNumber();
            ++cumulative_loss_;
            return true;
        }
        if(sequence_number > received_seq_max_){
            // 丢包
            return false;
        }
        // 表示乱序
        return true;

    }

    void StreamStat::UpdateJitter(const webrtc::RtpPacketReceived &packet, int64_t receive_time) {

        // RTP包到达接收端时间差
        int64_t receive_diff_ms = receive_time - last_received_time_ms_;//R2-R1的时间
        uint32_t receive_diff_rtp = static_cast<uint32_t>(
                (receive_diff_ms * packet.payload_type_frequency()) / 1000); //换算成rtp时间
        int32_t time_diff_samples =
                receive_diff_rtp - (packet.Timestamp() - last_received_timestamp_);
        time_diff_samples = std::abs(time_diff_samples);
        // 5s,video: 90k
        if (time_diff_samples < 450000) {
            // Note we calculate in Q4 to avoid using float.
            int32_t jitter_diff_q4 = (time_diff_samples << 4) - jitter_q4_;
            jitter_q4_ += ((jitter_diff_q4 + 8) >> 4);
        }
    }

    void StreamStat::MaybeAppendReportBlockAndReset(std::vector<webrtc::rtcp::ReportBlock> &result) {
        int64_t now_ms = clock_->TimeInMilliseconds();
        // 如果最近8s没有接受到数据
        if(now_ms - last_received_time_ms_ > kStreamStatTimeoutMs){
            return;
        }
        // 如果没有收到任何数据包
        if(!ReceiveRtpPacket()){
            return;
        }
        result.emplace_back();
        webrtc::rtcp::ReportBlock& stats = result.back();
        stats.SetMediaSsrc(ssrc_);
        // 设置丢包指数
        // 计算期望收到的数据包个数
        int64_t exp_since_last = received_seq_max_ - last_report_seq_max_ + 1;
        int32_t loss_since_last = cumulative_loss_ - last_report_cumulative_loss_;
        if(exp_since_last > 0 && loss_since_last > 0){
            stats.SetFractionLost((loss_since_last << 8) / exp_since_last);
        }
        // 设置累计丢包的个数
        int32_t packets_lost = cumulative_loss_ + cumulative_loss_rtcp_offset_;
        if(packets_lost < 0){
            packets_lost = 0;
            cumulative_loss_rtcp_offset_ = -cumulative_loss_;
        }
        if(packets_lost > 0x7FFFFF){
            if(!cumulative_loss_is_capped_){
                cumulative_loss_is_capped_ = true;
                RTC_LOG(LS_WARNING) << "cumulative packet loss reached max value for ssrc: " << ssrc_;
            }
            packets_lost = 0x7FFFFF;
        }
        stats.SetCumulativeLost(packets_lost);
        stats.SetExtHighestSeqNum(received_seq_max_);
        stats.SetJitter(jitter_q4_ >> 4);

        last_report_seq_max_ = received_seq_max_;
        last_report_cumulative_loss_ = cumulative_loss_;

    }
}