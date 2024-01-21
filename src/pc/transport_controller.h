
#ifndef  __TRANSPORT_CONTROLLER_H_
#define  __TRANSPORT_CONTROLLER_H_

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
    
    int set_local_description(SessionDescription* desc);
    int set_remote_description(SessionDescription* desc);
    void set_local_certificate(rtc::RTCCertificate* cert);
    int send_rtp(const std::string& transport_name, const char* data, size_t len);
    int send_rtcp(const std::string& transport_name, const char* data, size_t len);
    void set_dtls(bool is_dtls){
        is_dtls_ = is_dtls;
    }

    sigslot::signal4<TransportController*, const std::string&, IceCandidateComponent,
        const std::vector<Candidate>&> signal_candidate_allocate_done;
    sigslot::signal2<TransportController*, PeerConnectionState> signal_connection_state;
    sigslot::signal3<TransportController*, rtc::CopyOnWriteBuffer*, int64_t>
        signal_rtp_packet_received;
    sigslot::signal3<TransportController*, rtc::CopyOnWriteBuffer*, int64_t>
        signal_rtcp_packet_received;

private:
    void on_candidate_allocate_done(IceAgent* agent,
            const std::string& transport_name,
            IceCandidateComponent component,
            const std::vector<Candidate>& candidates);
    void _on_dtls_receiving_state(DtlsTransport*);
    void _on_dtls_writable_state(DtlsTransport*);
    void _on_dtls_state(DtlsTransport*, DtlsTransportState);
    void _on_ice_state(IceAgent*, IceTransportState);
    void _on_rtp_packet_received(DtlsSrtpTransport*,
        rtc::CopyOnWriteBuffer* packet, int64_t ts);
    void _on_rtcp_packet_received(DtlsSrtpTransport*,
            rtc::CopyOnWriteBuffer* packet, int64_t ts);
    void _update_state();
    void _add_dtls_transport(DtlsTransport* dtls);
    DtlsTransport* _get_dtls_transport(const std::string& transport_name);
    void _add_dtls_srtp_transport(DtlsSrtpTransport* dtls);
    DtlsSrtpTransport* _get_dtls_srtp_transport(const std::string& transport_name);

private:
    EventLoop* _el;
    IceAgent* _ice_agent;
    bool is_dtls_ = true;
    std::map<std::string, DtlsTransport*> _dtls_transport_by_name;
    std::map<std::string, DtlsSrtpTransport*> _dtls_srtp_transport_by_name;
    rtc::RTCCertificate* _local_certificate = nullptr;
    PeerConnectionState _pc_state = PeerConnectionState::k_new;
};

} // namespace xrtc

#endif  //__TRANSPORT_CONTROLLER_H_


