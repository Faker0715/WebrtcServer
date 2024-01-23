//
// Created by Faker on 2024/1/22.
//

#include "rtcp_sender.h"
#include "rtc_base/logging.h"

namespace xrtc {

    RTCPSender::RTCPSender(const RtpRtcpConfig &config) : clock_(config.clock_){

    }

    RTCPSender::~RTCPSender() {

    }

    void RTCPSender::SendRTCP(webrtc::RTCPPacketType packet_type) {
        auto result = ComputeCompoundRTCPPacket(packet_type);
    }

    absl::optional<int32_t> RTCPSender::ComputeCompoundRTCPPacket(webrtc::RTCPPacketType packet_type) {
        if (method_ == webrtc::RtcpMode::kOff){
            RTC_LOG(LS_WARNING) << "Can't send rtcp packet when rtcp is off";
        }
        SetFlag(packet_type,false);
        PrepareReport();
        return absl::nullopt;

    }

    void RTCPSender::SetRtcpStatus(webrtc::RtcpMode method) {
        method_ = method;
    }

    void RTCPSender::SetFlag(uint32_t type, bool is_valatile) {
        report_flags_.insert(ReportFlag(type,is_valatile));
    }

    bool RTCPSender::PrepareReport() {
        bool generate_report = false;
        if(isFlagPresent(webrtc::kRtcpSr) || IsFlagPresent(webrtc::kRtcpRr) {
            generate_report = true;
        }else {
            generate_report = (method_ == webrtc::RtcpMode::kReducedSize && ConsumeFlag(webrtc::kRtcpReport)) ||
                              (method_ == webrtc::RtcpMode::kCompound);
            if(generate_report){
                SetFlag(sending_? webrtc::kRtcpSr:webrtc::kRtcpRr, true
            }

        }
    }

    bool RTCPSender::ConsumeFlag(uint32_t type, bool force) {
        auto it = report_flags_.find(ReportFlag(type,false));
        if(it != report_flags_.end()){
            if(it->is_valatile || force){
                report_flags_.erase(it);
            }
            return true;
        }
        return false;
    }


    bool RTCPSender::IsFlagPresent(uint32_t type) {
        return report_flags_.find(ReportFlag(type,false)) != report_flags_.end();
    }
}

}
