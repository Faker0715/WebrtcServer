#ifndef  __XRTCSERVER_PC_TRANSPORT_CONTROLLER_H_
#define  __XRTCSERVER_PC_TRANSPORT_CONTROLLER_H_

#include <map>

#include "ice/ice_agent.h"
#include "pc/session_description.h"
#include "pc/peer_connection_def.h"

namespace xrtc {

class DtlsTransport;
enum class DtlsTransportState;
class DtlsSrtpTransport;

class TransportController : public sigslot::has_slots<> {
public:
    TransportController(EventLoop* el, PortAllocator* allocator);
    ~TransportController();
    
    int SetLocalDescription(SessionDescription* desc);
    int SetRemoteDescription(SessionDescription* desc);
    void SetLocalCertificate(rtc::RTCCertificate* cert);
    int SendRtp(const std::string& transport_name, const char* data, size_t len);
    int SendRtcp(const std::string& transport_name, const char* data, size_t len);

    void set_dtls(bool is_dtls) { is_dtls_ = is_dtls; }

    sigslot::signal4<TransportController*, const std::string&, IceCandidateComponent,
        const std::vector<Candidate>&> SignalCandidateAllocateDone;
    sigslot::signal2<TransportController*, PeerConnectionState> SignalConnectionState;
    sigslot::signal3<TransportController*, rtc::CopyOnWriteBuffer*, int64_t>
        SignalRtpPacketReceived;
    sigslot::signal3<TransportController*, rtc::CopyOnWriteBuffer*, int64_t>
        SignalRtcpPacketReceived;

private:
    void OnCandidateAllocateDone(IceAgent* agent,
            const std::string& transport_name,
            IceCandidateComponent component,
            const std::vector<Candidate>& candidates);
    void OnDtlsReceivingState(DtlsTransport*);
    void OnDtlsWritableState(DtlsTransport*);
    void OnDtlsState(DtlsTransport*, DtlsTransportState);
    void OnIceState(IceAgent*, IceTransportState);
    void OnRtpPacketReceived(DtlsSrtpTransport*,
        rtc::CopyOnWriteBuffer* packet, int64_t ts);
    void OnRtcpPacketReceived(DtlsSrtpTransport*,
            rtc::CopyOnWriteBuffer* packet, int64_t ts);
    void OnReadPacket(IceTransportChannel* ice_channel,
            const char* data,
            size_t len,
            int64_t ts);
    void UpdateState();
    void AddDtlsTransport(DtlsTransport* dtls);
    DtlsTransport* GetDtlsTransport(const std::string& transport_name);
    void AddDtlsSrtpTransport(DtlsSrtpTransport* dtls);
    DtlsSrtpTransport* GetDtlsSrtpTransport(const std::string& transport_name);

private:
    EventLoop* el_;
    bool is_dtls_ = true;
    IceAgent* ice_agent_;
    std::map<std::string, DtlsTransport*> dtls_transport_by_name_;
    std::map<std::string, DtlsSrtpTransport*> dtls_srtp_transport_by_name_;
    rtc::RTCCertificate* local_certificate_ = nullptr;
    PeerConnectionState pc_state_ = PeerConnectionState::kNew;
};

} // namespace xrtc

#endif  //__XRTCSERVER_PC_TRANSPORT_CONTROLLER_H_


