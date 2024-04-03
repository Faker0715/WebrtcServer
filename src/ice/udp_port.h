#ifndef  __XRTCSERVER_ICE_UDP_PORT_H_
#define  __XRTCSERVER_ICE_UDP_PORT_H_

#include <string>
#include <vector>
#include <map>

#include <rtc_base/socket_address.h>

#include "base/event_loop.h"
#include "base/network.h"
#include "base/async_udp_socket.h"
#include "ice/ice_def.h"
#include "ice/ice_credentials.h"
#include "ice/candidate.h"
#include "ice/stun.h"

namespace xrtc {

class IceConnection;

typedef std::map<rtc::SocketAddress, IceConnection*> AddressMap;

class UDPPort : public sigslot::has_slots<> {
public:
    UDPPort(EventLoop* el,
            const std::string& transport_name,
            IceCandidateComponent component,
            IceParameters ice_params);
    ~UDPPort();
        
    std::string ice_ufrag() { return ice_params_.ice_ufrag; }
    std::string ice_pwd() { return ice_params_.ice_pwd; }

    const std::string& transport_name() { return transport_name_; }
    IceCandidateComponent component() { return component_; }
    const rtc::SocketAddress& local_addr() { return local_addr_; }
    const std::vector<Candidate>& candidates() { return candidates_; }

    int CreateIceCandidate(Network* network, int min_port, int max_port, Candidate& c);
    bool GetStunMessage(const char* data, size_t len,
            const rtc::SocketAddress& addr,
            std::unique_ptr<StunMessage>* out_msg,
            std::string* out_username);
    void SendBindingErrorResponse(StunMessage* stun_msg,
            const rtc::SocketAddress& addr,
            int err_code,
            const std::string& reason);
    IceConnection* CreateConnection(const Candidate& candidate);
    IceConnection* GetConnection(const rtc::SocketAddress& addr);
    void CreateStunUsername(const std::string& remote_username, 
            std::string* stun_attr_username);

    int SendTo(const char* buf, size_t len, const rtc::SocketAddress& addr);

    std::string ToString();
    
    sigslot::signal4<UDPPort*, const rtc::SocketAddress&, StunMessage*, const std::string&>
        SignalUnknownAddress;

private:
    void OnReadPacket(AsyncUdpSocket* socket, char* buf, size_t size,
            const rtc::SocketAddress& addr, int64_t ts);
    bool ParseStunUsername(StunMessage* stun_msg, std::string* local_ufrag,
            std::string* remote_frag);

private:
    EventLoop* el_;
    std::string transport_name_;
    IceCandidateComponent component_;
    IceParameters ice_params_;
    int socket_ = -1;
    std::unique_ptr<AsyncUdpSocket> async_socket_;
    rtc::SocketAddress local_addr_;
    std::vector<Candidate> candidates_;
    AddressMap connections_;
};

} // namespace xrtc

#endif  //__XRTCSERVER_ICE_UDP_PORT_H_


