//
// Created by faker on 23-4-2.
//

#include "push_stream.h"
namespace xrtc{
    PushStream::PushStream(xrtc::EventLoop *el,PortAllocator* allocator,  uint64_t uid, const std::string &stream_name, bool audio, bool video,
                                 uint32_t log_id) : RtcStream(el, allocator,uid, stream_name, audio, video, log_id) {

    }

    PushStream::~PushStream() {

    }

    std::string PushStream::create_offer() {
        RTCOfferAnswerOptions options;
        options.send_audio = false;
        options.send_video = false;
        options.recv_audio = audio;
        options.recv_video = video;
//        options.use_rtcp_mux = false;
        return pc->create_offer(options);
    }
}
