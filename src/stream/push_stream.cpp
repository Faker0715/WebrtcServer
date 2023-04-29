//
// Created by faker on 23-4-2.
//

#include "push_stream.h"
#include <rtc_base/logging.h>
namespace xrtc{
    PushStream::PushStream(xrtc::EventLoop *el,PortAllocator* allocator,  uint64_t uid, const std::string &stream_name, bool audio, bool video,
                                 uint32_t log_id) : RtcStream(el, allocator,uid, stream_name, audio, video, log_id) {

    }

    PushStream::~PushStream() {
        RTC_LOG(LS_INFO) << to_string() << ": Push stream destroy";

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

    bool PushStream::get_audio_source(std::vector<StreamParams> &source) {
        return _get_source("audio",source);
    }

    bool PushStream::get_video_source(std::vector<StreamParams> &source) {
        return _get_source("video",source);
    }

    bool PushStream::_get_source(const std::string &mid, std::vector<StreamParams> &source) {
        if(!pc){
            return false;
        }
        auto remote_desc = pc->remote_desc();
        if(!remote_desc){
            return false;
        }
        auto content = remote_desc->get_content(mid);
        if(!content){
            return false;
        }
        source = content->streams();
        return true;

    }
}
