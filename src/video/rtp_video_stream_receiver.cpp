//
// Created by faker on 24-1-30.
//

#include "rtp_video_stream_receiver.h"

namespace xrtc {

    RtpVideoStreamReceiver::RtpVideoStreamReceiver(const VideoReceiveStreamConfig &config,
                                                   ReceiveStat *rtp_receive_stat) :
            config_(config),
            rtp_receive_stat_(rtp_receive_stat){

    }

    RtpVideoStreamReceiver::~RtpVideoStreamReceiver() {

    }

    void RtpVideoStreamReceiver::OnRtpPacket(const webrtc::RtpPacketReceived &packet) {
        // 重传包不统计
        if(!packet.recovered()){
            rtp_receive_stat_->OnRtpPacket(packet);
        }

    }
}