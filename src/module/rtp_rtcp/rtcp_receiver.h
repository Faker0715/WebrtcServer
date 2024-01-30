//
// Created by Faker on 2024/1/30.
//

#ifndef XRTCSERVER_RTCP_RECEIVER_H
#define XRTCSERVER_RTCP_RECEIVER_H

#include "rtp_rtcp_config.h"
#include "api/array_view.h"

namespace xrtc{
    class RTCPReceiver{
    public:
        RTCPReceiver(const RtpRtcpConfig& config);
        ~RTCPReceiver();
        void IncomingRtcpPacket(const uint8_t* packet, size_t palength);
        void IncomingRtcpPacket(rtc::ArrayView<const uint8_t> packet);
    };

}


#endif //XRTCSERVER_RTCP_RECEIVER_H
