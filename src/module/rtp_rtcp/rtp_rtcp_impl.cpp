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
        RTC_LOG(LS_WARNING) << "TimeToSendRTCP---";
        rtcp_sender_.SendRTCP(webrtc::kRtcpReport);
    }
}
