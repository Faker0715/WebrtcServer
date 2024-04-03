#include "stream/rtc_stream.h"

#include <rtc_base/logging.h>

namespace xrtc {

const size_t kIceTimeout = 30000; // 30s;

RtcStream::RtcStream(EventLoop* el, PortAllocator* allocator,
        uint64_t uid, const std::string& stream_name,
        bool audio, bool video, uint32_t log_id):
    el(el), uid(uid), stream_name(stream_name), audio(audio),
    video(video), log_id(log_id),
    pc(new PeerConnection(el, allocator))
{
    pc->SignalConnectionState.connect(this, &RtcStream::OnConnectionState);
    pc->SignalRtpPacketReceived.connect(this, &RtcStream::OnRtpPacketReceived);
    pc->SignalRtcpPacketReceived.connect(this, &RtcStream::OnRtcpPacketReceived);
}

RtcStream::~RtcStream() {
    if (ice_timeout_watcher_) {
        el->DeleteTimer(ice_timeout_watcher_);
        ice_timeout_watcher_ = nullptr;
    }

    pc->Destroy();
}

void RtcStream::OnConnectionState(PeerConnection*, PeerConnectionState state) {
    if (state_ == state) {
        return;
    }

    RTC_LOG(LS_INFO) << ToString() << ": PeerConnectionState change from " << state_
        << " to " << state;
    state_ = state;
    
    if (state_ == PeerConnectionState::kConnected) {
        if (ice_timeout_watcher_) {
            el->DeleteTimer(ice_timeout_watcher_);
            ice_timeout_watcher_ = nullptr;
        }
    }

    if (listener_) {
        listener_->OnConnectionState(this, state);
    }
}

void RtcStream::OnRtpPacketReceived(PeerConnection*, 
        rtc::CopyOnWriteBuffer* packet, int64_t /*ts*/)
{
    if (listener_) {
        listener_->OnRtpPacketReceived(this, (const char*)packet->data(), packet->size());
    }
}

void RtcStream::OnRtcpPacketReceived(PeerConnection*, 
        rtc::CopyOnWriteBuffer* packet, int64_t /*ts*/)
{
    if (listener_) {
        listener_->OnRtcpPacketReceived(this, (const char*)packet->data(), packet->size());
    }
}

void IceTimeoutCb(EventLoop* /*el*/, TimerWatcher* /*w*/, void* data) {
    RtcStream* stream = (RtcStream*)data;
    if (stream->state_ != PeerConnectionState::kConnected) {
        if (stream->listener_) {
            stream->listener_->OnStreamException(stream);
        }
    }
}

int RtcStream::Start(rtc::RTCCertificate* certificate) {
    ice_timeout_watcher_ = el->CreateTimer(IceTimeoutCb, this, false);
    el->StartTimer(ice_timeout_watcher_, kIceTimeout * 1000);

    return pc->Init(certificate);
}

int RtcStream::SetRemoteSdp(const std::string& sdp) {
    return pc->SetRemoteSdp(sdp);
}

int RtcStream::SendRtp(const char* data, size_t len) {
    if (pc) {
        return pc->SendRtp(data, len);
    }
    return -1;
}

int RtcStream::SendRtcp(const char* data, size_t len) {
    if (pc) {
        return pc->SendRtcp(data, len);
    }
    return -1;
}

std::string RtcStream::ToString() {
    std::stringstream ss;
    ss << "Stream[" << this << "|" << uid << "|" << stream_name << "]";
    return ss.str();
}

} // namespace xrtc


