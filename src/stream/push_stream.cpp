#include "stream/push_stream.h"

#include <rtc_base/logging.h>

namespace xrtc {

PushStream::PushStream(EventLoop* el, PortAllocator* allocator, 
        uint64_t uid, const std::string& stream_name,
        bool audio, bool video, uint32_t log_id) :
    RtcStream(el, allocator, uid, stream_name, audio, video, log_id)
{
}

PushStream::~PushStream() {
    RTC_LOG(LS_INFO) << ToString() << ": Push stream destroy.";
}

std::string PushStream::CreateOffer() {
    RTCOfferAnswerOptions options;
    options.send_audio = false;
    options.send_video = false;
    options.recv_audio = audio;
    options.recv_video = video;

    return pc->CreateOffer(options);
}

bool PushStream::GetAudioSource(std::vector<StreamParams>& source) {
    return GetSource("audio", source);
}

bool PushStream::GetVideoSource(std::vector<StreamParams>& source) {
    return GetSource("video", source);
}

bool PushStream::GetSource(const std::string& mid, std::vector<StreamParams>& source) {
    if (!pc) {
        return false;
    }

    auto remote_desc = pc->remote_desc();
    if (!remote_desc) {
        return false;
    }

    auto content = remote_desc->GetContent(mid);
    if (!content) {
        return false;
    }

    source = content->streams();
    return true;
}

} // namespace xrtc


