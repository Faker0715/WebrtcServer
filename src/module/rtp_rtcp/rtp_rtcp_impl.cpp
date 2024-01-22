//
// Created by Faker on 2024/1/22.
//

#include "rtp_rtcp_impl.h"

namespace xrtc {

    xrtc::RtpRtcpImpl::~RtpRtcpImpl() {

    }

    xrtc::RtpRtcpImpl::RtpRtcpImpl(const RtpRtcpConfig &config) : el_(config.el_),
                                                                  rtcp_sender_(config) {

    }
}
