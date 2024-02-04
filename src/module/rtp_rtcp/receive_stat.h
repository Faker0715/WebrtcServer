//
// Created by faker on 24-2-3.
//

#ifndef XRTCSERVER_RECEIVE_STAT_H
#define XRTCSERVER_RECEIVE_STAT_H

#include "system_wrappers/include/clock.h"
#include "modules/rtp_rtcp/source/rtp_packet_received.h"
#include <rtc_base/containers/flat_map.h>

namespace xrtc{
    class StreamStat{
    public:
        StreamStat(uint32_t ssrc, webrtc::Clock* clock);
        ~StreamStat();
        void UpdateCounters(const webrtc::RtpPacketReceived& packet);
    private:
        uint32_t ssrc_;
        webrtc::Clock* clock_;

    };
    class ReceiveStat{
    public:
        ReceiveStat(webrtc::Clock* clock);
        ~ReceiveStat();
        static std::unique_ptr<ReceiveStat> Create(webrtc::Clock* clock);
        void OnRtpPacket(const webrtc::RtpPacketReceived& packet);
        // 如果有就创建 没有就返回
        StreamStat* GetOrCreateStat(uint32_t ssrc);
    private:
        webrtc::Clock* clock_;
        // 需求主要是查询
        webrtc::flat_map<uint32_t, std::unique_ptr<StreamStat>> stats_;
    };
}


#endif //XRTCSERVER_RECEIVE_STAT_H
