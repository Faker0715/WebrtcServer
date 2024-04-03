#include "stream/pull_stream.h"

#include <rtc_base/logging.h>

namespace xrtc {

PullStream::PullStream(EventLoop* el, PortAllocator* allocator, 
        uint64_t uid, const std::string& stream_name,
        bool audio, bool video, uint32_t log_id) :
    RtcStream(el, allocator, uid, stream_name, audio, video, log_id)
{
}

PullStream::~PullStream() {
    RTC_LOG(LS_INFO) << ToString() << ": Pull stream destroy.";
}

std::string PullStream::CreateOffer() {
    RTCOfferAnswerOptions options;
    options.send_audio = audio;
    options.send_video = video;
    options.recv_audio = false;
    options.recv_video = false;

    return pc->CreateOffer(options);
}

void PullStream::AddAudioSource(const std::vector<StreamParams>& source) {
    if (pc) {
        pc->AddAudioSource(source);
    }
}

void PullStream::AddVideoSource(const std::vector<StreamParams>& source) {
    if (pc) {
        pc->AddVideoSource(source);
    }
}

} // namespace xrtc


