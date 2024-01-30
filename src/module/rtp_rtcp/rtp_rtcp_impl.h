//
// Created by Faker on 2024/1/22.
//

#ifndef XRTCSERVER_RTP_RTCP_IMPL_H
#define XRTCSERVER_RTP_RTCP_IMPL_H

#include "base/event_loop.h"
#include "rtp_rtcp_config.h"
#include "rtcp_sender.h"
#include "rtcp_receiver.h"

namespace xrtc{
    class RtpRtcpImpl{
    public:
        RtpRtcpImpl(const RtpRtcpConfig& config);
        ~RtpRtcpImpl();
        void SetRTCPStatus(webrtc::RtcpMode method);
        void TimeToSendRTCP();
    private:
        EventLoop* el_;
        RTCPSender rtcp_sender_;
        RTCPReceiver rtcp_receiver_;
        TimerWatcher* rtcp_report_time_ = nullptr;
    };
}




#endif //XRTCSERVER_RTP_RTCP_IMPL_H
