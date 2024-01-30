//
// Created by faker on 24-1-30.
//

#include "rtp_video_stream_receiver.h"

namespace xrtc {

    RtpVideoStreamReceiver::RtpVideoStreamReceiver(const VideoReceiveStreamConfig &config) :
            config_(config) {

    }

    RtpVideoStreamReceiver::~RtpVideoStreamReceiver() {

    }
}