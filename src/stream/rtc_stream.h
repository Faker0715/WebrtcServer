#ifndef  __XRTCSERVER_STREAM_RTC_STREAM_H_
#define  __XRTCSERVER_STREAM_RTC_STREAM_H_

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
    virtual void OnConnectionState(RtcStream* stream, PeerConnectionState state) = 0;
    virtual void OnRtpPacketReceived(RtcStream* stream, const char* data, size_t len) = 0;
    virtual void OnRtcpPacketReceived(RtcStream* stream, const char* data, size_t len) = 0;
    virtual void OnStreamException(RtcStream* stream) = 0;
};

class RtcStream : public sigslot::has_slots<> {
public:
    RtcStream(EventLoop* el, PortAllocator* allocator, uint64_t uid, 
            const std::string& stream_name, bool audio, bool video, 
            uint32_t log_id);
    virtual ~RtcStream();
    
    int Start(rtc::RTCCertificate* certificate);
    int SetRemoteSdp(const std::string& sdp);
    void RegisterListener(RtcStreamListener* listener) { listener_ = listener; }

    virtual std::string CreateOffer() = 0;
    virtual RtcStreamType stream_type() = 0;
    
    uint64_t get_uid() { return uid; }
    const std::string& get_stream_name() { return stream_name; }
    
    int SendRtp(const char* data, size_t len);
    int SendRtcp(const char* data, size_t len);

    std::string ToString();

private:
    void OnConnectionState(PeerConnection*, PeerConnectionState);
    void OnRtpPacketReceived(PeerConnection*, 
        rtc::CopyOnWriteBuffer* packet, int64_t /*ts*/);
    void OnRtcpPacketReceived(PeerConnection*, 
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
    PeerConnectionState state_ = PeerConnectionState::kNew;
    RtcStreamListener* listener_ = nullptr;
    TimerWatcher* ice_timeout_watcher_ = nullptr;

    friend class RtcStreamManager;
    friend void IceTimeoutCb(EventLoop* el, TimerWatcher* w, void* data);

};

} // namespace xrtc

#endif  //__XRTCSERVER_STREAM_RTC_STREAM_H_


