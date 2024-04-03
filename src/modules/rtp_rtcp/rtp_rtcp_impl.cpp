#include "modules/rtp_rtcp/rtp_rtcp_impl.h"

#include <rtc_base/logging.h>

#include "base/conf.h"

extern xrtc::GeneralConf* g_conf;

namespace xrtc {

namespace {

void RtcpReportCb(EventLoop* /*el*/, TimerWatcher* /*w*/, void* data) {
    RtpRtcpImpl* rtp_rtcp = (RtpRtcpImpl*)data;
    rtp_rtcp->TimeToSendRTCP();
}

} // namespace

RtpRtcpImpl::RtpRtcpImpl(const RtpRtcpConfig& config)
    : el_(config.el),
    rtcp_sender_(config),
    rtcp_receiver_(config)
{

}

RtpRtcpImpl::~RtpRtcpImpl() {
    if (rtcp_report_timer_) {
        el_->DeleteTimer(rtcp_report_timer_);
        rtcp_report_timer_ = nullptr;
    }
}

void RtpRtcpImpl::TimeToSendRTCP() {
    rtcp_sender_.SendRTCP(GetFeedbackState(), webrtc::kRtcpReport);
    uint32_t interval = rtcp_sender_.cur_report_interval_ms();
    RTC_LOG(LS_WARNING) << "=======cur report interval ms: " << interval;
    el_->StartTimer(rtcp_report_timer_, interval * 1000);
}

void RtpRtcpImpl::SetRTCPStatus(webrtc::RtcpMode method) {
    if (method == webrtc::RtcpMode::kOff) {
        if (rtcp_report_timer_) {
            el_->DeleteTimer(rtcp_report_timer_);
            rtcp_report_timer_ = nullptr;
        }
    } else {
        if (!rtcp_report_timer_) {
            rtcp_report_timer_ = el_->CreateTimer(RtcpReportCb, this, true);
            el_->StartTimer(rtcp_report_timer_, g_conf->rtcp_report_timer_interval * 1000);
        }
    }

    rtcp_sender_.SetRtcpStatus(method);
}

void RtpRtcpImpl::IncomingRtcpPacket(const uint8_t* data, size_t len) {
    rtcp_receiver_.IncomingRtcpPacket(data, len);
}

void RtpRtcpImpl::SetRemoteSsrc(uint32_t ssrc) {
    rtcp_receiver_.SetRemoteSsrc(ssrc);
}

RTCPSender::FeedbackState RtpRtcpImpl::GetFeedbackState() {
    RTCPSender::FeedbackState state;
    
    uint32_t receive_ntp_secs;
    uint32_t receive_ntp_frac;
    state.remote_sr = 0;

    if (rtcp_receiver_.NTP(&receive_ntp_secs,
                           &receive_ntp_frac,
                           &state.last_rr_ntp_secs,
                           &state.last_rr_ntp_frac,
                           nullptr))
    {
        state.remote_sr = ((receive_ntp_secs & 0x0000FFFF) << 16) +
                    ((receive_ntp_frac & 0xFFFF0000) >> 16);
    }

    return state;
}

} // namespace xrtc

















