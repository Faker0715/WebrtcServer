//
// Created by Faker on 2024/1/30.
//

#include "rtcp_receiver.h"
namespace xrtc{

    RTCPReceiver::RTCPReceiver(const RtpRtcpConfig &config) {
    }

    RTCPReceiver::~RTCPReceiver() {

    }

    void RTCPReceiver::IncomingRtcpPacket(const uint8_t *packet, size_t palength) {
        IncomingRtcpPacket(rtc::MakeArrayView<const uint8_t>(packet,palength));
    }

    void RTCPReceiver::IncomingRtcpPacket(rtc::ArrayView<const uint8_t> packet) {

    }
}
