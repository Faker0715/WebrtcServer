#include "modules/rtp_rtcp/rtcp_sender.h"

#include <rtc_base/logging.h>
#include <modules/rtp_rtcp/source/rtcp_packet/receiver_report.h>
#include <modules/rtp_rtcp/source/rtp_rtcp_config.h>
#include <modules/rtp_rtcp/source/time_util.h>

namespace xrtc {
namespace {

const int kDefaultAudioReportInterval = 5000;
const int kDefaultVideoReportInterval = 1000;

} // namespace

class RTCPSender::PacketSender {
public:
    PacketSender(size_t max_packet_size, 
            webrtc::rtcp::RtcpPacket::PacketReadyCallback callback) :
        max_packet_size_(max_packet_size), callback_(callback) {}
    ~PacketSender() {}
    
    void AppendPacket(const webrtc::rtcp::RtcpPacket& packet) {
        packet.Create(buffer_, &index_, max_packet_size_, callback_);
    }

    void Send() {
        if (index_ > 0) {
            callback_(rtc::ArrayView<const uint8_t>(buffer_, index_));
            index_ = 0;
        }
    }

private:
    size_t max_packet_size_;
    webrtc::rtcp::RtcpPacket::PacketReadyCallback callback_;
    size_t index_ = 0;
    uint8_t buffer_[IP_PACKET_SIZE];
};

RTCPSender::RTCPSender(const RtpRtcpConfig& config) :
    clock_(config.clock),
    audio_(config.audio),
    ssrc_(config.local_media_ssrc),
    receive_stat_(config.receive_stat),
    max_packet_size_(IP_PACKET_SIZE - 28),
    report_interval_ms_(config.rtcp_report_interval_ms.value_or(
                audio_ ? kDefaultAudioReportInterval :
                         kDefaultVideoReportInterval)),
    cur_report_interval_ms_(report_interval_ms_ / 2),
    random_(clock_->TimeInMicroseconds()),
    rtp_rtcp_module_observer_(config.rtp_rtcp_module_observer)
{
    builders_[webrtc::kRtcpRr] = &RTCPSender::BuildRR;
}

RTCPSender::~RTCPSender() {

}

void RTCPSender::SetRtcpStatus(webrtc::RtcpMode method) {
    method_ = method;
}

void RTCPSender::SetFlag(uint32_t type, bool is_volatile) {
    report_flags_.insert(ReportFlag(type, is_volatile));
}

bool RTCPSender::IsFlagPresent(uint32_t type) {
    return report_flags_.find(ReportFlag(type, false)) != report_flags_.end();
}

bool RTCPSender::ConsumeFlag(uint32_t type, bool force) {
    auto it = report_flags_.find(ReportFlag(type, false));
    if (it == report_flags_.end()) {
        return false;
    }

    if (it->is_volatile || force) {
        report_flags_.erase(it);
    }

    return true;
}

int RTCPSender::SendRTCP(const FeedbackState& feedback_state,
        webrtc::RTCPPacketType packet_type) {
    auto callback = [&](rtc::ArrayView<const uint8_t> packet) {
        RTC_LOG(LS_WARNING) << "=======build rtcp packet, size: " << packet.size();
        if (rtp_rtcp_module_observer_) {
            rtp_rtcp_module_observer_->OnLocalRtcpPacket(
                    audio_ ? webrtc::MediaType::AUDIO : webrtc::MediaType::VIDEO,
                    packet.data(), packet.size());
        }
    };
    
    absl::optional<PacketSender> sender;
    sender.emplace(max_packet_size_, callback);

    auto result = ComputeCompoundRTCPPacket(feedback_state, packet_type, *sender);
    if (result) {
        return *result;
    }

    sender->Send();
    return 0;
}

absl::optional<int32_t> RTCPSender::ComputeCompoundRTCPPacket(
        const FeedbackState& feedback_state,
        webrtc::RTCPPacketType packet_type,
        PacketSender& sender)
{
    if (method_ == webrtc::RtcpMode::kOff) {
        RTC_LOG(LS_WARNING) << "cannot send rtcp if it is disabled";
        return -1;
    }
    
    SetFlag(packet_type, true);
    
    PrepareReport();
    
    // 遍历report flag，生成相应类型的rtcp包
    auto it = report_flags_.begin();
    while (it != report_flags_.end()) {
        uint32_t rtcp_packet_type = it->type;
        if (it->is_volatile) {
            report_flags_.erase(it++);
        } else {
            ++it;
        }

        auto builder_it = builders_.find(rtcp_packet_type);
        if (builder_it == builders_.end()) {
            RTC_LOG(LS_WARNING) << "could not build rtcp packet for packet type: "
                << it->type;
        } else {
            BuilderFunc func = builder_it->second;
            (this->*func)(feedback_state, sender);
        }
    }

    return absl::nullopt;
}

void RTCPSender::PrepareReport() {
    bool generate_report;
    if (IsFlagPresent(webrtc::kRtcpSr) || IsFlagPresent(webrtc::kRtcpRr)) {
        generate_report = true;
    } else {
        generate_report = (ConsumeFlag(webrtc::kRtcpReport) && 
                (method_ == webrtc::RtcpMode::kReducedSize)) ||
                (method_ == webrtc::RtcpMode::kCompound);

        if (generate_report) {
            SetFlag(sending_ ? webrtc::kRtcpSr : webrtc::kRtcpRr, true);
        }
    }

    uint32_t min_interval = report_interval_ms_;
    cur_report_interval_ms_ = random_.Rand(min_interval * 1 / 2, min_interval * 3 / 2); 
}

std::vector<webrtc::rtcp::ReportBlock> RTCPSender::CreateRtcpReportBlocks(
        const FeedbackState& feedback_state) 
{
    std::vector<webrtc::rtcp::ReportBlock> result;
    if (!receive_stat_) {
        return result;
    }
    
    result = receive_stat_->RtcpReportBlocks(webrtc::RTCP_MAX_REPORT_BLOCKS);
    
    // 进一步设置lastSr和delaySinceLastSr
    if (!result.empty() && ((feedback_state.last_rr_ntp_secs > 0) ||
            (feedback_state.last_rr_ntp_frac > 0)))
    {
        // 计算delay since last sr
        // 当前发送RR包时，接收端压缩后的32位NTP时间
        int32_t now = webrtc::CompactNtp(clock_->CurrentNtpTime());
        // 收到最近一次SR包时，接收端压缩后的32位NTP时间
        int32_t receive_time = feedback_state.last_rr_ntp_secs & 0x0000FFFF;
        receive_time <<= 16;
        receive_time += ((feedback_state.last_rr_ntp_frac & 0xFFFF0000) >> 16);
        int32_t delay_since_last_sr = now - receive_time;

        for (auto& report_block : result) {
            report_block.SetLastSr(feedback_state.remote_sr);
            report_block.SetDelayLastSr(delay_since_last_sr);
        }
    }

    return result;
}

void RTCPSender::BuildRR(const FeedbackState& feedback_state, PacketSender& sender) {  
    webrtc::rtcp::ReceiverReport rr;
    rr.SetSenderSsrc(ssrc_);
    rr.SetReportBlocks(CreateRtcpReportBlocks(feedback_state));
    sender.AppendPacket(rr);
}

} // namespace xrtc

















