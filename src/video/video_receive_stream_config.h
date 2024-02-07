//
// Created by faker on 24-1-30.
//

#ifndef XRTCSERVER_VIDEO_RECEIVE_STREAM_CONFIG_H
#define XRTCSERVER_VIDEO_RECEIVE_STREAM_CONFIG_H
#include <system_wrappers/include/clock.h>
#include "base/event_loop.h"
namespace xrtc{
class VideoReceiveStreamConfig{
public:
    EventLoop* el = nullptr;
    webrtc::Clock *clock = nullptr;
    struct Rtp{
        uint32_t local_ssrc = 0;
    }rtp;
};
}


#endif //XRTCSERVER_VIDEO_RECEIVE_STREAM_CONFIG_H
