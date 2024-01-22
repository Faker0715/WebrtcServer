//
// Created by Faker on 2024/1/22.
//

#ifndef XRTCSERVER_RTP_RTCP_IMPL_H
#define XRTCSERVER_RTP_RTCP_IMPL_H

#include "base/event_loop.h"
#include "rtp_rtcp_config.h"

namespace xrtc{
    class RtpRtcpImpl{
    public:
        RtpRtcpImpl(const RtpRtcpConfig& config);
        ~RtpRtcpImpl();
    private:
        EventLoop* el_;
    };
}




#endif //XRTCSERVER_RTP_RTCP_IMPL_H
