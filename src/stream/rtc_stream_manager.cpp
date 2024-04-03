#include "stream/rtc_stream_manager.h"

#include <rtc_base/logging.h>

#include "base/conf.h"
#include "stream/push_stream.h"
#include "stream/pull_stream.h"

extern xrtc::GeneralConf* g_conf;

namespace xrtc {

RtcStreamManager::RtcStreamManager(EventLoop* el) :
    el_(el),
    allocator_(new PortAllocator())
{
    allocator_->SetPortRange(g_conf->ice_min_port, g_conf->ice_max_port);
}

RtcStreamManager::~RtcStreamManager() {
}

PushStream* RtcStreamManager::FindPushStream(const std::string& stream_name) {
    auto iter = push_streams_.find(stream_name);
    if (iter != push_streams_.end()) {
        return iter->second;
    }

    return nullptr;
}

PullStream* RtcStreamManager::FindPullStream(const std::string& stream_name) {
    auto iter = pull_streams_.find(stream_name);
    if (iter != pull_streams_.end()) {
        return iter->second;
    }

    return nullptr;
}

void RtcStreamManager::RemovePushStream(RtcStream* stream) {
    if (!stream) {
        return;
    }

    RemovePushStream(stream->get_uid(), stream->get_stream_name());
}

void RtcStreamManager::RemovePushStream(uint64_t uid, const std::string& stream_name) {
    PushStream* push_stream = FindPushStream(stream_name);
    if (push_stream && uid == push_stream->get_uid()) {
        push_streams_.erase(stream_name);
        delete push_stream;
    }
}

void RtcStreamManager::RemovePullStream(RtcStream* stream) {
    if (!stream) {
        return;
    }

    RemovePullStream(stream->get_uid(), stream->get_stream_name());
}

void RtcStreamManager::RemovePullStream(uint64_t uid, const std::string& stream_name) {
    PullStream* pull_stream = FindPullStream(stream_name);
    if (pull_stream && uid == pull_stream->get_uid()) {
        pull_streams_.erase(stream_name);
        delete pull_stream;
    }
}

int RtcStreamManager::CreatePushStream(uint64_t uid, const std::string& stream_name,
        bool audio, bool video, 
        bool is_dtls, uint32_t log_id,
        rtc::RTCCertificate* certificate,
        std::string& offer)
{
    PushStream* stream = FindPushStream(stream_name);
    if (stream) {
        push_streams_.erase(stream_name);
        delete stream;
    }

    stream = new PushStream(el_, allocator_.get(), uid, stream_name,
            audio, video, log_id);
    stream->RegisterListener(this);
    
    if (is_dtls) {
        stream->Start(certificate);
    } else {
        stream->Start(nullptr);
    }
    
    offer = stream->CreateOffer();
    
    push_streams_[stream_name] = stream;
    return 0;
}

int RtcStreamManager::CreatePullStream(uint64_t uid, const std::string& stream_name,
        bool audio, bool video, 
        bool is_dtls, uint32_t log_id,
        rtc::RTCCertificate* certificate,
        std::string& offer)
{
    PushStream* push_stream = FindPushStream(stream_name);
    if (!push_stream) {
        RTC_LOG(LS_WARNING) << "Stream not found, uid: " << uid << ", stream_name: "
            << stream_name << ", log_id: " << log_id;
        return -1;
    }
    
    RemovePullStream(uid, stream_name);
    
    std::vector<StreamParams> audio_source;
    std::vector<StreamParams> video_source;
    push_stream->GetAudioSource(audio_source);
    push_stream->GetVideoSource(video_source);

    PullStream* stream = new PullStream(el_, allocator_.get(), uid, stream_name,
            audio, video, log_id);
    stream->RegisterListener(this);
    stream->AddAudioSource(audio_source);
    stream->AddVideoSource(video_source);
    if (is_dtls) {
        stream->Start(certificate);
    } else {
        stream->Start(nullptr);
    }

    offer = stream->CreateOffer();
    
    pull_streams_[stream_name] = stream;
    return 0;
}

int RtcStreamManager::SetAnswer(uint64_t uid, const std::string& stream_name,
        const std::string& answer, const std::string& stream_type, 
        uint32_t log_id)
{
    if ("push" == stream_type) {
        PushStream* push_stream = FindPushStream(stream_name);
        if (!push_stream) {
            RTC_LOG(LS_WARNING) << "push stream not found, uid: " << uid
                << ", stream_name: " << stream_name
                << ", log_id: " << log_id;
            return -1;
        }

        if (uid != push_stream->uid) {
            RTC_LOG(LS_WARNING) << "uid invalid, uid: " << uid
                << ", stream_name: " << stream_name
                << ", log_id: " << log_id;
            return -1;
        }
        
        push_stream->SetRemoteSdp(answer);

    } else if ("pull" == stream_type) {
        PullStream* pull_stream = FindPullStream(stream_name);
        if (!pull_stream) {
            RTC_LOG(LS_WARNING) << "pull stream not found, uid: " << uid
                << ", stream_name: " << stream_name
                << ", log_id: " << log_id;
            return -1;
        }

        if (uid != pull_stream->uid) {
            RTC_LOG(LS_WARNING) << "uid invalid, uid: " << uid
                << ", stream_name: " << stream_name
                << ", log_id: " << log_id;
            return -1;
        }

        pull_stream->SetRemoteSdp(answer);
    }

    return 0;
}

int RtcStreamManager::StopPush(uint64_t uid, const std::string& stream_name) {
    RemovePushStream(uid, stream_name);
    return 0;
}

int RtcStreamManager::StopPull(uint64_t uid, const std::string& stream_name) {
    RemovePullStream(uid, stream_name);
    return 0;
}

void RtcStreamManager::OnConnectionState(RtcStream* stream, 
        PeerConnectionState state)
{
    if (state == PeerConnectionState::kFailed) {
        if (stream->stream_type() == RtcStreamType::k_push) {
            RemovePushStream(stream);
        } else if (stream->stream_type() == RtcStreamType::k_pull) {
            RemovePullStream(stream);
        }
    } 
}

void RtcStreamManager::OnRtpPacketReceived(RtcStream* stream, 
        const char* data, size_t len) 
{
    if (RtcStreamType::k_push == stream->stream_type()) {
        PullStream* pull_stream = FindPullStream(stream->get_stream_name());
        if (pull_stream) {
            pull_stream->SendRtp(data, len);
        }
    }
}

void RtcStreamManager::OnRtcpPacketReceived(RtcStream* stream, 
        const char* data, size_t len)
{
    if (RtcStreamType::k_push == stream->stream_type()) {
        PullStream* pull_stream = FindPullStream(stream->get_stream_name());
        if (pull_stream) {
            pull_stream->SendRtcp(data, len);
        }
    } else if (RtcStreamType::k_pull == stream->stream_type()) {
        PushStream* push_stream = FindPushStream(stream->get_stream_name());
        if (push_stream) {
            push_stream->SendRtcp(data, len);
        }
    }
}

void RtcStreamManager::OnStreamException(RtcStream* stream) {
    if (RtcStreamType::k_push == stream->stream_type()) {
        RemovePushStream(stream);
    } else if (RtcStreamType::k_pull == stream->stream_type()) {
        RemovePullStream(stream);
    }
}

} // namespace xrtc


