//
// Created by Faker on 2024/1/22.
//

#include "rtcp_sender.h"

namespace xrtc {

    RTCPSender::RTCPSender(const RtpRtcpConfig &config) : clock_(config.clock_){

    }

    RTCPSender::~RTCPSender() {

    }

    void RTCPSender::SendRTCP(webrtc::RTCPPacketType packet_type) {

    }

}
