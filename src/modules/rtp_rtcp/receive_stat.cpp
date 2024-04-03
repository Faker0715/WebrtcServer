#include "modules/rtp_rtcp/receive_stat.h"

#include <rtc_base/logging.h>

namespace xrtc {

namespace {

const int kMaxReorderingThreshold = 450;
const int kStreamStatTimeoutMs = 8000;

} // namespace

StreamStat::StreamStat(uint32_t ssrc, webrtc::Clock* clock) :
    ssrc_(ssrc),
    clock_(clock),
    max_reordering_threshold_(kMaxReorderingThreshold)
{
}

StreamStat::~StreamStat() {
}

void StreamStat::UpdateCounters(const webrtc::RtpPacketReceived& packet) {
    int64_t now_ms = clock_->TimeInMilliseconds();
    receive_counters_.transmitted.AddPacket(packet);
    --cumulative_loss_;

    int64_t sequence_number = seq_unwrapper_.UnwrapWithoutUpdate(packet.SequenceNumber()); 
    // 收到第一个包
    if (!ReceivedRtpPacket()) {
        received_seq_first_ = sequence_number;
        received_seq_max_ = sequence_number - 1;
    } else if (UpdateOutOfOrder(packet, sequence_number, now_ms)) { // 发生乱序
        return;
    }
    
    // 顺序到达的rtp包
    cumulative_loss_ += (sequence_number - received_seq_max_);
    received_seq_max_ = sequence_number;

    seq_unwrapper_.UpdateLast(sequence_number);
    
    // 计算jitter
    if (packet.Timestamp() != last_received_timestamp_ && (
                receive_counters_.transmitted.packets -
                receive_counters_.retransmitted.packets) > 1)
    {
        UpdateJitter(packet, now_ms);
    }

    last_received_timestamp_ = packet.Timestamp();
    last_received_time_ms_ = now_ms;
}

void StreamStat::UpdateJitter(const webrtc::RtpPacketReceived& packet, 
        int64_t receive_time)
{
    // RTP包到达接收端时间差
    int64_t receive_time_diff = receive_time - last_received_time_ms_;
    uint32_t receive_rtp_diff = static_cast<uint32_t>(receive_time_diff * 
        packet.payload_type_frequency() / 1000);
   
    // D(i, j) = (Rj - Ri) - (Sj- Si) = (Rj - Sj) - (Ri - Si)
    int32_t time_diff_samples = receive_rtp_diff - 
        (packet.Timestamp() - last_received_timestamp_);

    time_diff_samples = std::abs(time_diff_samples);

    // 5s, video: 90k
    if (time_diff_samples < 450000) {
        // (|D(i - 1, i)| - J(i - 1)) 
        int32_t jitter_q4_diff = (time_diff_samples << 4) - jitter_q4_;
        // J(i) = J(i - 1) + (|D(i - 1, i)| - J(i - 1)) / 16
        jitter_q4_ += ((jitter_q4_diff + 8) >> 4);
    }
}

bool StreamStat::UpdateOutOfOrder(const webrtc::RtpPacketReceived& packet,
        int64_t sequence_number,
        int64_t /*now_ms*/)
{ 
    // 检测是否发生突变
    if (received_seq_out_of_order_) {
        uint16_t expected_seq_num = *received_seq_out_of_order_ + 1;
        received_seq_out_of_order_ = absl::nullopt;
        if (packet.SequenceNumber() == expected_seq_num) { // 认定发生序列号突变，重置计数状态
            received_seq_max_ = sequence_number - 2;
            return false;
        }
    }
    
    // 有可能是流序列号发生突变
    if (abs(sequence_number - received_seq_max_) > max_reordering_threshold_) {
        received_seq_out_of_order_ = packet.SequenceNumber();
        ++cumulative_loss_;
        return true;
    }

    if (sequence_number > received_seq_max_) { // 丢包
        return false;
    }
    
    // 表示乱序
    return true;
}

void StreamStat::MaybeAppendReportBlockAndReset(
        std::vector<webrtc::rtcp::ReportBlock>& result)
{
    int64_t now_ms = clock_->TimeInMilliseconds();
    if (now_ms - last_received_time_ms_ > kStreamStatTimeoutMs) {
        return;
    }

    if (!ReceivedRtpPacket()) {
        return;
    }

    // 生成report block
    result.emplace_back();
    webrtc::rtcp::ReportBlock& stats = result.back();
    stats.SetMediaSsrc(ssrc_);

    // 设置丢包指数fraction lost
    // 计算期望收到的数据包个数
    int64_t exp_since_last = received_seq_max_ - last_report_seq_max_;
    // 计算最近一段时间内的丢包数
    int32_t loss_since_last = cumulative_loss_ - last_report_cumulative_loss_;
    if (exp_since_last > 0 && loss_since_last > 0) {
        stats.SetFractionLost(255 * loss_since_last / exp_since_last);
    }

    // 设置累计丢包的个数
    int32_t packets_lost = cumulative_loss_ + cumulative_loss_rtcp_offset_;
    if (packets_lost < 0) {
        packets_lost = 0;
        cumulative_loss_rtcp_offset_ = -cumulative_loss_;
    }

    if (packets_lost > 0x7FFFFF) {
        if (!cumulative_loss_is_capped_) {
            cumulative_loss_is_capped_ = true;
            RTC_LOG(LS_WARNING) << "cumulative packet loss reached max value for ssrc: " 
                << ssrc_;
        }
        packets_lost = 0x7FFFFF;
    }

    stats.SetCumulativeLost(packets_lost);
    stats.SetExtHighestSeqNum(received_seq_max_);
    stats.SetJitter(jitter_q4_ >> 4);

    last_report_seq_max_ = received_seq_max_;
    last_report_cumulative_loss_ = cumulative_loss_;
}

ReceiveStat::ReceiveStat(webrtc::Clock* clock) :
    clock_(clock)
{
}
    
ReceiveStat::~ReceiveStat() {
}

std::unique_ptr<ReceiveStat> ReceiveStat::Create(webrtc::Clock* clock) {
    return std::make_unique<ReceiveStat>(clock);
}

StreamStat* ReceiveStat::GetOrCreateStat(uint32_t ssrc) {
    std::unique_ptr<StreamStat>& stat = stats_[ssrc];
    if (nullptr == stat) {
        stat = std::make_unique<StreamStat>(ssrc, clock_);
        all_ssrcs_.push_back(ssrc);
    }

    return stat.get();
}

void ReceiveStat::OnRtpPacket(const webrtc::RtpPacketReceived& packet) {
    //RTC_LOG(LS_WARNING) << "=======ssrc: " << packet.Ssrc()
    //    << ", seq: " << packet.SequenceNumber();
    GetOrCreateStat(packet.Ssrc())->UpdateCounters(packet);
}

std::vector<webrtc::rtcp::ReportBlock> ReceiveStat::RtcpReportBlocks(size_t max_blocks) {
    std::vector<webrtc::rtcp::ReportBlock> result;
    result.reserve(std::min(max_blocks, all_ssrcs_.size()));

    size_t ssrc_idx = 0;
    for (size_t i = 0; i < all_ssrcs_.size() && result.size() < max_blocks; ++i) {
        ssrc_idx = (last_returned_ssrc_idx_ + 1 + i) % all_ssrcs_.size();
        uint32_t media_ssrc = all_ssrcs_[ssrc_idx];
        auto stat_it = stats_.find(media_ssrc);
        stat_it->second->MaybeAppendReportBlockAndReset(result);
    }
    
    last_returned_ssrc_idx_ = ssrc_idx;

    return result;
}

} // namespace xrtc


