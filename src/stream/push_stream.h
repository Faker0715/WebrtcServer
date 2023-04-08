//
// Created by faker on 23-4-2.
//

#ifndef XRTCSERVER_PUSH_STREAM_H
#define XRTCSERVER_PUSH_STREAM_H

#include "rtc_stream.h"
#include "ice/port_allocator.h"

namespace xrtc{
    class PushStream: public RtcStream{
    public:
PushStream(EventLoop* el,PortAllocator* allocator, uint64_t uid,const std::string& stream_name,
                   bool audio,bool video,uint32_t log_id);
        ~PushStream() override;
        std::string create_offer() override;
    };
}


#endif //XRTCSERVER_PUSH_STREAM_H
