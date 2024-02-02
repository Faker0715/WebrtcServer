//
// Created by faker on 24-2-3.
//

#include "receive_stat.h"
#include "rtc_base/logging.h"

namespace xrtc{

    ReceiveStat::ReceiveStat(webrtc::Clock *clock): clock_(clock) {

    }

    ReceiveStat::~ReceiveStat() {

    }

    void ReceiveStat::OnRtpPacket(const webrtc::RtpPacketReceived &packet) {

        RTC_LOG(LS_INFO) << "ReceiveStat::OnRtpPacket";
    }

    std::unique_ptr<ReceiveStat> ReceiveStat::Create(webrtc::Clock *clock) {
        return std::make_unique<ReceiveStat>(clock);
    }
}