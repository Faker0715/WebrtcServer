#ifndef  __XRTCSERVER_STREAM_RTC_STREAM_MANAGER_H_
#define  __XRTCSERVER_STREAM_RTC_STREAM_MANAGER_H_

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
    
    int CreatePushStream(uint64_t uid, const std::string& stream_name,
            bool audio, bool video, 
            bool is_dtls, uint32_t log_id,
            rtc::RTCCertificate* certificate,
            std::string& offer);
    
    int CreatePullStream(uint64_t uid, const std::string& stream_name,
            bool audio, bool video, 
            bool is_dtls, uint32_t log_id,
            rtc::RTCCertificate* certificate,
            std::string& offer);

    int SetAnswer(uint64_t uid, const std::string& stream_name,
            const std::string& answer, const std::string& stream_type, 
            uint32_t log_id);
    int StopPush(uint64_t uid, const std::string& stream_name);
    int StopPull(uint64_t uid, const std::string& stream_name);
 
    void OnConnectionState(RtcStream* stream, PeerConnectionState state) override;
    void OnRtpPacketReceived(RtcStream* stream, const char* data, size_t len) override;
    void OnRtcpPacketReceived(RtcStream* stream, const char* data, size_t len) override;
    void OnStreamException(RtcStream* stream) override;

private:
    PushStream* FindPushStream(const std::string& stream_name);
    void RemovePushStream(RtcStream* stream);
    void RemovePushStream(uint64_t uid, const std::string& stream_name);
    PullStream* FindPullStream(const std::string& stream_name);
    void RemovePullStream(RtcStream* stream);
    void RemovePullStream(uint64_t uid, const std::string& stream_name);

private:
    EventLoop* el_;
    std::unordered_map<std::string, PushStream*> push_streams_;
    std::unordered_map<std::string, PullStream*> pull_streams_;
    std::unique_ptr<PortAllocator> allocator_;
};

} // namespace xrtc


#endif  //__XRTCSERVER_STREAM_RTC_STREAM_MANAGER_H_


