//
// Created by faker on 23-4-2.
//

#include "pull_stream.h"
#include <rtc_base/logging.h>

namespace xrtc {
    PullStream::PullStream(xrtc::EventLoop *el, PortAllocator *allocator, uint64_t uid, const std::string &stream_name,
                           bool audio, bool video,
                           uint32_t log_id) : RtcStream(el, allocator, uid, stream_name, audio, video, log_id) {

    }

    PullStream::~PullStream() {
        RTC_LOG(LS_INFO) << to_string() << ": Pull stream destroy";

    }

    std::string PullStream::create_offer() {
        RTCOfferAnswerOptions options;
        options.send_audio = audio;
        options.send_video = video;
        options.recv_audio = false;
        options.recv_video = false;
//        options.use_rtcp_mux = false;
        return pc->create_offer(options);
    }

    void PullStream::add_audio_source(const std::vector<StreamParams> &source) {
        if(pc){
            pc->add_audio_source(source);
        }
    }

    void PullStream::add_video_source(const std::vector<StreamParams> &source) {
        if(pc){
            pc->add_video_source(source);
        }
    }

}
