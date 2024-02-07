//
// Created by faker on 24-1-30.
//

#include "rtp_video_stream_receiver.h"

namespace xrtc {

    namespace {
        std::unique_ptr<RtpRtcpImpl> CreateRtpRtcpModule(const VideoReceiveStreamConfig &vconf) {
            RtpRtcpConfig config;
            config.el_ = vconf.el;
            config.clock_ = vconf.clock;
            config.local_media_ssrc = vconf.rtp.local_ssrc;
            auto rtp_rtcp = std::make_unique<RtpRtcpImpl>(config);
            rtp_rtcp->SetRTCPStatus(webrtc::RtcpMode::kCompound);
            return rtp_rtcp;
        }
    }

    RtpVideoStreamReceiver::~RtpVideoStreamReceiver() {


    }

    void RtpVideoStreamReceiver::OnRtpPacket(const webrtc::RtpPacketReceived &packet) {
        // 重传包不统计
        if(!packet.recovered()){
            rtp_receive_stat_->OnRtpPacket(packet);
        }

    }

    RtpVideoStreamReceiver::RtpVideoStreamReceiver(const VideoReceiveStreamConfig &config,
                                                   ReceiveStat *rtp_receive_stat):
                                                   config_(config),
                                                   rtp_receive_stat_(rtp_receive_stat),
                                                   rtp_rtcp_(CreateRtpRtcpModule(config)){

    }
}