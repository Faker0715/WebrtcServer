#ifndef  __XRTCSERVER_MODULES_RTP_RTCP_RTP_RTCP_CONFIG_H_
#define  __XRTCSERVER_MODULES_RTP_RTCP_RTP_RTCP_CONFIG_H_

#include <system_wrappers/include/clock.h>

#include "base/event_loop.h"
#include "modules/rtp_rtcp/receive_stat.h"

namespace xrtc {

class RtpRtcpModuleObserver {
public:
    virtual ~RtpRtcpModuleObserver() {}

    virtual void OnLocalRtcpPacket(webrtc::MediaType media_type,
            const uint8_t* data, size_t len) = 0;
};

struct RtpRtcpConfig {
    EventLoop* el = nullptr;
    webrtc::Clock* clock = nullptr;
    bool audio = false;
    uint32_t local_media_ssrc = 0;
    ReceiveStat* receive_stat = nullptr;
    absl::optional<uint32_t> rtcp_report_interval_ms;
    RtpRtcpModuleObserver* rtp_rtcp_module_observer = nullptr;
};

} // namespace xrtc

#endif  //__XRTCSERVER_MODULES_RTP_RTCP_RTP_RTCP_CONFIG_H_


