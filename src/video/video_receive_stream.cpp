//
// Created by faker on 24-1-30.
//

#include "video_receive_stream.h"

namespace xrtc {

    VideoReceiveStream::VideoReceiveStream(const VideoReceiveStreamConfig &config):
            config_(config),
            rtp_video_stream_receiver(config){

    }

    VideoReceiveStream::~VideoReceiveStream() {

    }
}