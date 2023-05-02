
#ifndef  __RTC_STREAM_MANAGER_H_
#define  __RTC_STREAM_MANAGER_H_

#include <string>
#include <unordered_map>

#include <rtc_base/rtc_certificate.h>

#include "ice/port_allocator.h"
#include "base/event_loop.h"
#include "stream/rtc_stream.h"

namespace xrtc {

class PushStream;
class PullStream;

class RtcStreamManager : public RtcStreamListener {
public:
    RtcStreamManager(EventLoop* el);
    ~RtcStreamManager();
    
    int create_push_stream(uint64_t uid, const std::string& stream_name,
            bool audio, bool video, uint32_t log_id,
            rtc::RTCCertificate* certificate,
            std::string& offer);
    
    int create_pull_stream(uint64_t uid, const std::string& stream_name,
            bool audio, bool video, uint32_t log_id,
            rtc::RTCCertificate* certificate,
            std::string& offer);

    int set_answer(uint64_t uid, const std::string& stream_name,
            const std::string& answer, const std::string& stream_type, 
            uint32_t log_id);
    int stop_push(uint64_t uid, const std::string& stream_name);
 
    void on_connection_state(RtcStream* stream, PeerConnectionState state) override;
    void on_rtp_packet_received(RtcStream* stream, const char* data, size_t len) override;
    void on_rtcp_packet_received(RtcStream* stream, const char* data, size_t len) override;

private:
    PushStream* _find_push_stream(const std::string& stream_name);
    void _remove_push_stream(RtcStream* stream);
    void _remove_push_stream(uint64_t uid, const std::string& stream_name);
    PullStream* _find_pull_stream(const std::string& stream_name);
    void _remove_pull_stream(RtcStream* stream);
    void _remove_pull_stream(uint64_t uid, const std::string& stream_name);

private:
    EventLoop* _el;
    std::unordered_map<std::string, PushStream*> _push_streams;
    std::unordered_map<std::string, PullStream*> _pull_streams;
    std::unique_ptr<PortAllocator> _allocator;
};

} // namespace xrtc


#endif  //__RTC_STREAM_MANAGER_H_


