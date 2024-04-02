//
// Created by Faker on 2024/1/22.
//

#ifndef XRTCSERVER_RTP_RTCP_CONFIG_H
#define XRTCSERVER_RTP_RTCP_CONFIG_H

#include <system_wrappers/include/clock.h>
#include "base/event_loop.h"
#include "receive_stat.h"
namespace xrtc{
    class RtpRtcpModuleObserver{
    public:
        virtual ~RtpRtcpModuleObserver() {};
        virtual void OnLocalRtcpPacket(webrtc::MediaType mediaType,
                                       const uint8_t* data,size_t len) = 0;
    };
    struct RtpRtcpConfig{
        EventLoop* el_ = nullptr;
        webrtc::Clock* clock_ = nullptr;
        bool audio = false;
        uint32_t local_media_ssrc = 0;
        ReceiveStat* receive_stat = nullptr;
        absl::optional<uint32_t> rtcp_report_interval_ms;
        RtpRtcpModuleObserver* rtp_rtcp_module_observer = nullptr;
    };
}


#endif //XRTCSERVER_RTP_RTCP_CONFIG_H
