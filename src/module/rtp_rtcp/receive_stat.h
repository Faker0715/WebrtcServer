//
// Created by faker on 24-2-3.
//

#ifndef XRTCSERVER_RECEIVE_STAT_H
#define XRTCSERVER_RECEIVE_STAT_H

#include "system_wrappers/include/clock.h"
#include "modules/rtp_rtcp/source/rtp_packet_received.h"

namespace xrtc{
    class ReceiveStat{
    public:
        ReceiveStat(webrtc::Clock* clock);
        ~ReceiveStat();
        static std::unique_ptr<ReceiveStat> Create(webrtc::Clock* clock);
        void OnRtpPacket(const webrtc::RtpPacketReceived& packet);
    private:
        webrtc::Clock* clock_;
    };
}


#endif //XRTCSERVER_RECEIVE_STAT_H
