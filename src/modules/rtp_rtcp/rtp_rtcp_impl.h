#ifndef  __XRTCSERVER_MODULES_RTP_RTCP_RTP_RTCP_IMPL_H_
#define  __XRTCSERVER_MODULES_RTP_RTCP_RTP_RTCP_IMPL_H_

#include "modules/rtp_rtcp/rtp_rtcp_config.h"
#include "modules/rtp_rtcp/rtcp_sender.h"
#include "modules/rtp_rtcp/rtcp_receiver.h"

namespace xrtc {

class RtpRtcpImpl {
public:
    RtpRtcpImpl(const RtpRtcpConfig& config);
    ~RtpRtcpImpl();
    
    void SetRTCPStatus(webrtc::RtcpMode method);
    void TimeToSendRTCP();
    void IncomingRtcpPacket(const uint8_t* data, size_t len);
    void SetRemoteSsrc(uint32_t ssrc);

private:
    RTCPSender::FeedbackState GetFeedbackState();

private:
    EventLoop* el_;
    RTCPSender rtcp_sender_;
    RTCPReceiver rtcp_receiver_;

    TimerWatcher* rtcp_report_timer_ = nullptr;
};

} // namespace xrtc


#endif  // __XRTCSERVER_MODULES_RTP_RTCP_RTP_RTCP_IMPL_H_


