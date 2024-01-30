//
// Created by faker on 24-1-30.
//

#ifndef XRTCSERVER_VIDEO_RECEIVE_STREAM_H
#define XRTCSERVER_VIDEO_RECEIVE_STREAM_H
#include "video_receive_stream_config.h"
#include "rtp_video_stream_receiver.h"

namespace xrtc {

    class VideoReceiveStream{
    public:
        VideoReceiveStream(const VideoReceiveStreamConfig& config);
        ~VideoReceiveStream();
    private:
        VideoReceiveStreamConfig config_;
        RtpVideoStreamReceiver rtp_video_stream_receiver;
    };
}


#endif //XRTCSERVER_VIDEO_RECEIVE_STREAM_H
