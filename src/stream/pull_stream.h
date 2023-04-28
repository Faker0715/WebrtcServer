//
// Created by faker on 23-4-2.
//

#ifndef XRTCSERVER_PULL_STREAM_H
#define XRTCSERVER_PULL_STREAM_H

#include "rtc_stream.h"
#include "ice/port_allocator.h"

namespace xrtc{
    class PullStream: public RtcStream{
    public:
        PullStream(EventLoop* el,PortAllocator* allocator, uint64_t uid,const std::string& stream_name,
                   bool audio,bool video,uint32_t log_id);
        ~PullStream() override;
        std::string create_offer() override;
        RtcStreamType stream_type() override{
            return RtcStreamType::k_pull;
        };
    };
}


#endif //XRTCSERVER_PULL_STREAM_H