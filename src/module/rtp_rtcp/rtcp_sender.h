//
// Created by Faker on 2024/1/22.
//

#ifndef XRTCSERVER_RTCP_SENDER_H
#define XRTCSERVER_RTCP_SENDER_H
#include "module/rtp_rtcp/rtp_rtcp_config.h"
#include <modules/rtp_rtcp/include/rtp_rtcp_defines.h>
#include <set>

namespace xrtc{
    class RTCPSender{
    public:
        RTCPSender(const RtpRtcpConfig& config);
        ~RTCPSender();
        void SendRTCP(webrtc::RTCPPacketType packet_type);
        void SetRtcpStatus(webrtc::RtcpMode method);
        void SetFlag(uint32_t type,bool is_valatile);
    private:
        absl::optional<int32_t> ComputeCompoundRTCPPacket(webrtc::RTCPPacketType packet_type);
    private:
        webrtc::Clock* clock_;
        webrtc::RtcpMode method_ = webrtc::RtcpMode::kOff;
        struct ReportFlag{
            ReportFlag(uint32_t type,bool is_valatile):type(type),is_valatile(is_valatile){}
            bool operator<(const ReportFlag& other) const{
                return type < other.type;
            }
            bool operator==(const ReportFlag& other) const{
                return type == other.type;
            }
            uint32_t type;
            // 不会消费一次被删除
            bool is_valatile;
        };
        // 当前发送的rtcp包的类型
        std::set<ReportFlag> report_flags_;
    };
}

#endif //XRTCSERVER_RTCP_SENDER_H
