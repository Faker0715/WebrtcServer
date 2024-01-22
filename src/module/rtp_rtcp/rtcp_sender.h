//
// Created by Faker on 2024/1/22.
//

#ifndef XRTCSERVER_RTCP_SENDER_H
#define XRTCSERVER_RTCP_SENDER_H
#include "rtp_rtcp_config.h"
#include <modules/rtp_rtcp/include/rtp_rtcp_defines.h>
namespace xrtc{
    class RTCPSender{
    public:
        RTCPSender(const RtpRtcpConfig& config);
        ~RTCPSender();
        void SendRTCP(webrtc::RTCPPacketType packet_type);
    private:
        webrtc::Clock* clock_;
    };
}

#endif //XRTCSERVER_RTCP_SENDER_H
