#include "pc/transport_controller.h"

#include <rtc_base/logging.h>

#include "pc/dtls_transport.h"
#include "pc/dtls_srtp_transport.h"
#include "modules/rtp_rtcp/rtp_utils.h"

namespace xrtc {

TransportController::TransportController(EventLoop* el, PortAllocator* allocator) :
    el_(el),
    ice_agent_(new IceAgent(el, allocator))
{
    ice_agent_->SignalCandidateAllocateDone.connect(this,
            &TransportController::OnCandidateAllocateDone);
}

TransportController::~TransportController() {
    for (auto dtls_srtp : dtls_srtp_transport_by_name_) {
        delete dtls_srtp.second;
    }
    dtls_srtp_transport_by_name_.clear();

    for (auto dtls : dtls_transport_by_name_) {
        delete dtls.second;
    }
    dtls_transport_by_name_.clear();

    if (ice_agent_) {
        delete ice_agent_;
        ice_agent_ = nullptr;
    }
}

void TransportController::OnCandidateAllocateDone(IceAgent* /*agent*/,
        const std::string& transport_name,
        IceCandidateComponent component,
        const std::vector<Candidate>& candidates)
{
    SignalCandidateAllocateDone(this, transport_name, component, candidates);
}

int TransportController::SetLocalDescription(SessionDescription* desc) {
    if (!desc) {
        RTC_LOG(LS_WARNING) << "desc is null";
        return -1;
    }
    
    for (auto content : desc->contents()) {
        std::string mid = content->mid();
        if (desc->IsBundle(mid) && mid != desc->GetFirstBundleMid()) {
            continue;
        }

        ice_agent_->CreateChannel(el_, mid, IceCandidateComponent::RTP);
        auto td = desc->GetTransportInfo(mid);
        if (td) {
            ice_agent_->SetIceParams(mid, IceCandidateComponent::RTP,
                    IceParameters(td->ice_ufrag, td->ice_pwd));
        }
        
        if (is_dtls_) {
            DtlsTransport* dtls = new DtlsTransport(
                    ice_agent_->GetChannel(mid, IceCandidateComponent::RTP));
            dtls->SetLocalCertificate(local_certificate_);
            dtls->SignalReceivingState.connect(this, 
                    &TransportController::OnDtlsReceivingState);
            dtls->SignalWritableState.connect(this, 
                    &TransportController::OnDtlsWritableState);
            dtls->SignalDtlsState.connect(this, 
                    &TransportController::OnDtlsState);
            ice_agent_->SignalIceState.connect(this,
                    &TransportController::OnIceState);
            AddDtlsTransport(dtls);

            DtlsSrtpTransport* dtls_srtp = new DtlsSrtpTransport(dtls->transport_name(),
                    true);
            dtls_srtp->set_dtls_transports(dtls, nullptr);
            dtls_srtp->SignalRtpPacketReceived.connect(this,
                    &TransportController::OnRtpPacketReceived);
            dtls_srtp->SignalRtcpPacketReceived.connect(this,
                    &TransportController::OnRtcpPacketReceived);
            AddDtlsSrtpTransport(dtls_srtp);
        } else {
            auto ice_channel = ice_agent_->GetChannel(mid, IceCandidateComponent::RTP);
            if (ice_channel) {
                ice_agent_->SignalIceState.connect(this,
                        &TransportController::OnIceState);
                ice_channel->SignalReadPacket.connect(this,
                        &TransportController::OnReadPacket);
            }
        }
    }
    
    ice_agent_->GatheringCandidate();

    return 0;
}

void TransportController::OnReadPacket(IceTransportChannel* /*ice_channel*/,
        const char* data,
        size_t len,
        int64_t ts)
{
    auto array_view = rtc::MakeArrayView(data, len);
    RtpPacketType packet_type = InferRtpPacketType(array_view);
    if (packet_type == RtpPacketType::kUnknown) {
        return;
    }

    rtc::CopyOnWriteBuffer packet(data, len);
    if (packet_type == RtpPacketType::kRtcp) {
        SignalRtcpPacketReceived(this, &packet, ts);
    } else {
        SignalRtpPacketReceived(this, &packet, ts);
    }
}

void TransportController::OnRtpPacketReceived(DtlsSrtpTransport*,
        rtc::CopyOnWriteBuffer* packet, int64_t ts)
{
    SignalRtpPacketReceived(this, packet, ts);
}

void TransportController::OnRtcpPacketReceived(DtlsSrtpTransport*,
        rtc::CopyOnWriteBuffer* packet, int64_t ts)
{
    SignalRtcpPacketReceived(this, packet, ts);
}

void TransportController::OnDtlsReceivingState(DtlsTransport*) {
    UpdateState();
}

void TransportController::OnDtlsWritableState(DtlsTransport*) {
    UpdateState();
}

void TransportController::OnDtlsState(DtlsTransport*, DtlsTransportState) {
    UpdateState();
}

void TransportController::OnIceState(IceAgent*, IceTransportState ice_state) {
    if (is_dtls_) {
        UpdateState();
    } else { // 没有开启DTLS
        PeerConnectionState pc_state = PeerConnectionState::kNew;
        switch (ice_state) {
            case IceTransportState::kNew:
                break;
            case IceTransportState::kChecking:
                pc_state = PeerConnectionState::kConnecting;
                break;
            case IceTransportState::kConnected:
            case IceTransportState::kCompleted:
                pc_state = PeerConnectionState::kConnected;
                break;
            case IceTransportState::kDisconnected:
                pc_state = PeerConnectionState::kDisconnected;
                break;
            case IceTransportState::kFailed:
                pc_state = PeerConnectionState::kFailed;
                break;
            case IceTransportState::kClosed:
                pc_state = PeerConnectionState::kClosed;
                break;
            default:
                break;
        }
        
        if (pc_state_ != pc_state) {
            pc_state_ = pc_state;
            SignalConnectionState(this, pc_state);
        }
    }
}

void TransportController::UpdateState() {
    PeerConnectionState pc_state = PeerConnectionState::kNew;

    std::map<DtlsTransportState, int> dtls_state_counts;
    std::map<IceTransportState, int> ice_state_counts;
    auto iter = dtls_transport_by_name_.begin();
    for (; iter != dtls_transport_by_name_.end(); ++iter) {
        dtls_state_counts[iter->second->dtls_state()]++;
        ice_state_counts[iter->second->ice_channel()->state()]++;
    }

    int total_connected = ice_state_counts[IceTransportState::kConnected] + 
        dtls_state_counts[DtlsTransportState::kConnected];
    int total_dtls_connecting = dtls_state_counts[DtlsTransportState::kConnecting];
    int total_failed = ice_state_counts[IceTransportState::kFailed] + 
        dtls_state_counts[DtlsTransportState::kFailed];
    int total_closed = ice_state_counts[IceTransportState::kClosed] + 
        dtls_state_counts[DtlsTransportState::kClosed];
    int total_new = ice_state_counts[IceTransportState::kNew] + 
        dtls_state_counts[DtlsTransportState::kNew];
    int total_ice_checking = ice_state_counts[IceTransportState::kChecking];
    int total_ice_disconnected = ice_state_counts[IceTransportState::kDisconnected];
    int total_ice_completed = ice_state_counts[IceTransportState::kCompleted];
    int total_transports = dtls_transport_by_name_.size() * 2;

    if (total_failed > 0) {
        pc_state = PeerConnectionState::kFailed;
    } else if (total_ice_disconnected > 0) {
        pc_state = PeerConnectionState::kDisconnected;
    } else if (total_new + total_closed == total_transports) {
        pc_state = PeerConnectionState::kNew;
    } else if (total_ice_checking + total_dtls_connecting + total_new > 0) {
        pc_state = PeerConnectionState::kConnecting;
    } else if (total_connected + total_ice_completed + total_closed == total_transports) {
        pc_state = PeerConnectionState::kConnected;
    }

    if (pc_state_ != pc_state) {
        pc_state_ = pc_state;
        SignalConnectionState(this, pc_state);
    }
}

void TransportController::SetLocalCertificate(rtc::RTCCertificate* cert) {
    local_certificate_ = cert;
}

void TransportController::AddDtlsTransport(DtlsTransport* dtls) {
    auto iter = dtls_transport_by_name_.find(dtls->transport_name());
    if (iter != dtls_transport_by_name_.end()) {
        delete iter->second;
    }

    dtls_transport_by_name_[dtls->transport_name()] = dtls;
}

DtlsTransport* TransportController::GetDtlsTransport(const std::string& transport_name) {
    auto iter = dtls_transport_by_name_.find(transport_name);
    if (iter != dtls_transport_by_name_.end()) {
        return iter->second;
    }

    return nullptr;
}

void TransportController::AddDtlsSrtpTransport(DtlsSrtpTransport* dtls_srtp) {
    auto iter = dtls_srtp_transport_by_name_.find(dtls_srtp->transport_name());
    if (iter != dtls_srtp_transport_by_name_.end()) {
        delete iter->second;
    }

    dtls_srtp_transport_by_name_[dtls_srtp->transport_name()] = dtls_srtp;
}

DtlsSrtpTransport* TransportController::GetDtlsSrtpTransport(
        const std::string& transport_name) 
{
    auto iter = dtls_srtp_transport_by_name_.find(transport_name);
    if (iter != dtls_srtp_transport_by_name_.end()) {
        return iter->second;
    }

    return nullptr;
}

int TransportController::SetRemoteDescription(SessionDescription* desc) {
    if (!desc) {
        return -1;
    }

    for (auto content : desc->contents()) {
        std::string mid = content->mid();
        if (desc->IsBundle(mid) && mid != desc->GetFirstBundleMid()) {
            continue;
        }
        
        auto td = desc->GetTransportInfo(mid);
        if (td) {
            ice_agent_->SetRemoteIceParams(content->mid(), IceCandidateComponent::RTP,
                    IceParameters(td->ice_ufrag, td->ice_pwd));
            auto dtls = GetDtlsTransport(mid);
            if (dtls) {
                dtls->SetRemoteFingerprint(td->identity_fingerprint->algorithm,
                        td->identity_fingerprint->digest.cdata(),
                        td->identity_fingerprint->digest.size());
            }
        }
    }

    return 0;
}

int TransportController::SendRtp(const std::string& transport_name, 
        const char* data, size_t len)
{
    if (is_dtls_) {
        auto dtls_srtp = GetDtlsSrtpTransport(transport_name);
        if (dtls_srtp) {
            return dtls_srtp->SendRtp(data, len);
        }
    } else {
        auto ice_channel = ice_agent_->GetChannel(transport_name, IceCandidateComponent::RTP);
        if (ice_channel) {
            ice_channel->SendPacket(data, len);
        }
    }
    return -1;
}

int TransportController::SendRtcp(const std::string& transport_name, 
        const char* data, size_t len)
{
    if (is_dtls_) {
        auto dtls_srtp = GetDtlsSrtpTransport(transport_name);
        if (dtls_srtp) {
            return dtls_srtp->SendRtcp(data, len);
        }
    } else {
        auto ice_channel = ice_agent_->GetChannel(transport_name, IceCandidateComponent::RTP);
        if (ice_channel) {
            ice_channel->SendPacket(data, len);
        }
    }
    return -1;
}

} // namespace xrtc


