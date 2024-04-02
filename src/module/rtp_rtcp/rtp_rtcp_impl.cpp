//
// Created by Faker on 2024/1/22.
//

#include "rtp_rtcp_impl.h"
#include "base/conf.h"
#include "rtc_base/logging.h"
extern xrtc::GeneralConf* g_conf;
namespace xrtc {

    namespace{
        void RtcpReportCb(EventLoop* /*el*/,TimerWatcher* /*w*/,void* data){
            RtpRtcpImpl* rtp_rtcp = static_cast<RtpRtcpImpl*>(data);
            rtp_rtcp->TimeToSendRTCP();
        }
    }
    xrtc::RtpRtcpImpl::~RtpRtcpImpl() {
        if(rtcp_report_time_){
            el_->delete_timer(rtcp_report_time_);
            rtcp_report_time_ = nullptr;
        }
    }

    xrtc::RtpRtcpImpl::RtpRtcpImpl(const RtpRtcpConfig &config) : el_(config.el_),
                                                                  rtcp_sender_(config),
                                                                  rtcp_receiver_(config){

    }

    void RtpRtcpImpl::SetRTCPStatus(webrtc::RtcpMode method) {
        if(method == webrtc::RtcpMode::kOff) {
            if (rtcp_report_time_) {
                el_->delete_timer(rtcp_report_time_);
                rtcp_report_time_ = nullptr;
            }
        }else{
            if(!rtcp_report_time_){
                rtcp_report_time_ = el_->create_timer(RtcpReportCb,this,true);
                el_->start_timer(rtcp_report_time_,g_conf->rtcp_report_timer_interval*1000);
            }
        }
        rtcp_sender_.SetRtcpStatus(method);
    }

    void RtpRtcpImpl::TimeToSendRTCP() {
        rtcp_sender_.SendRTCP(GetFeedbackState(),webrtc::kRtcpReport);
        uint32_t interval = rtcp_sender_.cur_report_interval_ms();
        RTC_LOG(LS_WARNING) << "====rtcp report interval: " << interval;
        el_->start_timer(rtcp_report_time_,interval * 1000);
    }

    void RtpRtcpImpl::IncomingRTCPPacket(const uint8_t *data, size_t length) {
        rtcp_receiver_.IncomingRtcpPacket(data,length);
    }

    void RtpRtcpImpl::SetRemoteSSRC(uint32_t ssrc) {
        rtcp_receiver_.SetRemoteSSRC(ssrc);
    }

    RTCPSender::FeedbackState RtpRtcpImpl::GetFeedbackState() {
        RTCPSender::FeedbackState state;
        uint32_t receive_ntp_secs;
        uint32_t receive_ntp_frac;
        state.remote_sr = 0;
        if(rtcp_receiver_.NTP(&receive_ntp_secs,
                              &receive_ntp_frac,
                              &state.last_rr_ntp_secs,
                              &state.last_rr_ntp_frac,
                              nullptr)){
            state.remote_sr = ((receive_ntp_secs & 0x0000FFFF) << 16) +
                    ((receive_ntp_frac & 0xFFFF0000)>> 16);
        }



        return state;
    }
}
