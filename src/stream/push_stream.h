#ifndef  __XRTCSERVER_STREAM_PUSH_STREAM_H_
#define  __XRTCSERVER_STREAM_PUSH_STREAM_H_

#include "stream/rtc_stream.h"

namespace xrtc {

class PushStream : public RtcStream {
public:
    PushStream(EventLoop* el, PortAllocator* allocator, uint64_t uid, 
            const std::string& stream_name,
            bool audio, bool video, uint32_t log_id);
    ~PushStream() override;

    std::string CreateOffer() override;
    RtcStreamType stream_type() override { return RtcStreamType::k_push; }

    bool GetAudioSource(std::vector<StreamParams>& source);
    bool GetVideoSource(std::vector<StreamParams>& source);

private:
    bool GetSource(const std::string& mid, std::vector<StreamParams>& source);
};

} // namespace xrtc

#endif  //__XRTCSERVER_STREAM_PUSH_STREAM_H_


