
#ifndef  __RTC_STREAM_H_
#define  __RTC_STREAM_H_

#include <string>
#include <memory>

#include <rtc_base/rtc_certificate.h>

#include "base/event_loop.h"
#include "pc/peer_connection.h"

namespace xrtc {

class RtcStream;

enum class RtcStreamType {
    k_push,
    k_pull
};

class RtcStreamListener {
public:
    virtual void on_connection_state(RtcStream* stream, PeerConnectionState state) = 0;
    virtual void on_rtp_packet_received(RtcStream* stream, const char* data, size_t len) = 0;
    virtual void on_rtcp_packet_received(RtcStream* stream, const char* data, size_t len) = 0;
    virtual void on_stream_exception(RtcStream* stream) = 0;
};

class RtcStream : public sigslot::has_slots<> {
public:
    RtcStream(EventLoop* el, PortAllocator* allocator, uint64_t uid, 
            const std::string& stream_name, bool audio, bool video, 
            uint32_t log_id);
    virtual ~RtcStream();
    
    int start(rtc::RTCCertificate* certificate);
    int set_remote_sdp(const std::string& sdp);
    void register_listener(RtcStreamListener* listener) { _listener = listener; }

    virtual std::string create_offer() = 0;
    virtual RtcStreamType stream_type() = 0;
    
    uint64_t get_uid() { return uid; }
    const std::string& get_stream_name() { return stream_name; }
    
    int send_rtp(const char* data, size_t len);
    int send_rtcp(const char* data, size_t len);

    std::string to_string();

private:
    void _on_connection_state(PeerConnection*, PeerConnectionState);
    void _on_rtp_packet_received(PeerConnection*, 
        rtc::CopyOnWriteBuffer* packet, int64_t /*ts*/);
    void _on_rtcp_packet_received(PeerConnection*, 
            rtc::CopyOnWriteBuffer* packet, int64_t /*ts*/);

protected:
    EventLoop* el;
    uint64_t uid;
    std::string stream_name;
    bool audio;
    bool video;
    uint32_t log_id;

    PeerConnection* pc;
private:
    PeerConnectionState _state = PeerConnectionState::k_new;
    RtcStreamListener* _listener = nullptr;
    TimerWatcher* _ice_timeout_watcher = nullptr;

    friend class RtcStreamManager;
    friend void ice_timeout_cb(EventLoop *el, TimerWatcher *w, void *data);
};

} // namespace xrtc

#endif  //__RTC_STREAM_H_


