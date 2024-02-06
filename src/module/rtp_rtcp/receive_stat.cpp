//
// Created by faker on 24-2-3.
//

#include "receive_stat.h"
#include "rtc_base/logging.h"

namespace xrtc{
    const int kMaxReorderingThreshold = 450;
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
        }
        return stat.get();
    }

    StreamStat::StreamStat(uint32_t ssrc, webrtc::Clock *clock): ssrc_(ssrc), clock_(clock), max_reordering_threshold_(kMaxReorderingThreshold){

    }

    StreamStat::~StreamStat() {

    }

    void StreamStat::UpdateCounters(const webrtc::RtpPacketReceived &packet) {
        int64_t now_ms = clock_->TimeInMilliseconds();

        receive_counters_.transmitted.AddPacket(packet);
        --cumulative_loss;

        int64_t sequence_number = seq_unwrapper_.UnwrapWithoutUpdate(packet.SequenceNumber());
        // 收到第一个包
        if (!ReceiveRtpPacket()){
            received_seq_first_ = sequence_number;
            received_seq_max_ = sequence_number - 1;
        }else if(UpdateOutOfOrder(packet,sequence_number,now_ms)){
            return;
        }
        // 顺序到达rtp
        cumulative_loss += (sequence_number - received_seq_max_);
        received_seq_max_ = sequence_number;
        seq_unwrapper_.UpdateLast(sequence_number);
        // 计算jitter
        if(packet.Timestamp() != last_received_timestamp_ && (receive_counters_.transmitted.packets > - receive_counters_.retransmitted.packets) > 1){
            UpdateJitter(packet, now_ms);
        }
        last_received_timestamp_ = packet.Timestamp();
        last_received_frame_time_ms_ = now_ms;
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
            ++cumulative_loss;
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
        int64_t receive_time_diff = receive_time - last_received_frame_time_ms_;
        uint32_t receive_rtp_diff =static_cast<uint32_t> (receive_time_diff * packet.payload_type_frequency() / 1000);;
        int32_t time_diff_samples = receive_time_diff - (packet.Timestamp() - last_received_timestamp_);
        time_diff_samples = std::abs(time_diff_samples);
        // 5s,video: 90k
        if(time_diff_samples < 450000){
            int32_t jitter_q4_diff = (time_diff_samples << 4) - jitter_q4_;
            jitter_q4_ += ((jitter_q4_diff + 8) >> 4);
        }
    }
}