#ifndef  __XRTCSERVER_PC_PEER_CONNECTION_H_
#define  __XRTCSERVER_PC_PEER_CONNECTION_H_

#include <string>
#include <memory>

#include <rtc_base/rtc_certificate.h>
#include <api/media_types.h>

#include "base/event_loop.h"
#include "pc/session_description.h"
#include "pc/transport_controller.h"
#include "pc/stream_params.h"
#include "video/video_receive_stream.h"

namespace xrtc {

struct RTCOfferAnswerOptions {
    bool send_audio = true;
    bool send_video = true;
    bool recv_audio = true;
    bool recv_video = true;
    bool use_rtp_mux = true;
    bool use_rtcp_mux = true;
    bool dtls_on = true;
};

class PeerConnection : public sigslot::has_slots<>,
                       public RtpRtcpModuleObserver
{
public:
    PeerConnection(EventLoop* el, PortAllocator* allocator);
    
    int Init(rtc::RTCCertificate* certificate);
    void Destroy();
    std::string CreateOffer(const RTCOfferAnswerOptions& options);
    int SetRemoteSdp(const std::string& sdp);
    
    SessionDescription* remote_desc() { return remote_desc_.get(); }
    SessionDescription* local_desc() { return local_desc_.get(); }
    
    void AddAudioSource(const std::vector<StreamParams>& source) {
        audio_source_ = source;
    }
    
    void AddVideoSource(const std::vector<StreamParams>& source) {
        video_source_ = source;
    }
    
    int SendRtp(const char* data, size_t len);
    int SendRtcp(const char* data, size_t len);

    sigslot::signal2<PeerConnection*, PeerConnectionState>
        SignalConnectionState;
    sigslot::signal3<PeerConnection*, rtc::CopyOnWriteBuffer*, int64_t>
        SignalRtpPacketReceived;
    sigslot::signal3<PeerConnection*, rtc::CopyOnWriteBuffer*, int64_t>
        SignalRtcpPacketReceived;

private:
    ~PeerConnection();
    void OnCandidateAllocateDone(TransportController* transport_controller,
            const std::string& transport_name,
            IceCandidateComponent component,
            const std::vector<Candidate>& candidate);
    
    void OnConnectionState(TransportController*, PeerConnectionState state);
    void OnRtpPacketReceived(TransportController*,
            rtc::CopyOnWriteBuffer* packet, int64_t ts);
    void OnRtcpPacketReceived(TransportController*,
            rtc::CopyOnWriteBuffer* packet, int64_t ts);
    
    void OnLocalRtcpPacket(webrtc::MediaType media_type,
            const uint8_t* data, size_t len) override;

    webrtc::MediaType GetMediaType(uint32_t ssrc) const;
    void CreateVideoReceiveStream(VideoContentDescription* video_content);
    void OnFrame(std::unique_ptr<RtpFrameObject> frame) override;
    friend void DestroyTimerCb(EventLoop* el, TimerWatcher* w, void* data);

private:
    EventLoop* el_;
    webrtc::Clock* clock_;
    bool is_dtls_ = true;
    std::unique_ptr<SessionDescription> local_desc_;
    std::unique_ptr<SessionDescription> remote_desc_;
    rtc::RTCCertificate* certificate_ = nullptr;
    std::unique_ptr<TransportController> transport_controller_;
    TimerWatcher* destroy_timer_ = nullptr;
    std::vector<StreamParams> audio_source_;
    std::vector<StreamParams> video_source_;
    
    uint32_t remote_audio_ssrc_ = 0;
    uint32_t remote_video_ssrc_ = 0;
    uint32_t remote_video_rtx_ssrc_ = 0;

    std::unique_ptr<VideoReceiveStream> video_receive_stream_;
};

} // namespace xrtc

#endif  //__XRTCSERVER_PC_PEER_CONNECTION_H_


