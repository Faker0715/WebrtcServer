#ifndef  __XRTCSERVER_STREAM_PULL_STREAM_H_
#define  __XRTCSERVER_STREAM_PULL_STREAM_H_

#include "stream/rtc_stream.h"

namespace xrtc {

class PullStream : public RtcStream {
public:
    PullStream(EventLoop* el, PortAllocator* allocator, uint64_t uid, 
            const std::string& stream_name,
            bool audio, bool video, uint32_t log_id);
    ~PullStream() override;

    std::string CreateOffer() override;
    RtcStreamType stream_type() override { return RtcStreamType::k_pull; }

    void AddAudioSource(const std::vector<StreamParams>& source);
    void AddVideoSource(const std::vector<StreamParams>& source);
};

} // namespace xrtc

#endif  //__XRTCSERVER_STREAM_PULL_STREAM_H_


