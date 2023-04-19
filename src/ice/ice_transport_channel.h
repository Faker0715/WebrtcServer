//
// Created by faker on 23-4-4.
//

#ifndef XRTCSERVER_ICE_TRANSPORT_CHANNEL_H
#define XRTCSERVER_ICE_TRANSPORT_CHANNEL_H

#include <string>
#include <vector>
#include "ice_def.h"
#include "base/event_loop.h"
#include "port_allocator.h"
#include "ice_credentials.h"
#include "candidate.h"
#include "rtc_base/third_party/sigslot/sigslot.h"
#include "udp_port.h"

namespace xrtc{
    class UDPPort;
class IceTransportChannel: public sigslot::has_slots<>{
    public:
    IceTransportChannel(EventLoop* el, PortAllocator* allocator,const std::string& transport_name, IceCandidateComponent component);
    virtual ~IceTransportChannel();
    std::string transport_name() const { return _transport_name; }
    IceCandidateComponent component() const { return _component; }
    void set_ice_params(const IceParameters ice_params);
    void set_remote_ice_params(const IceParameters ice_params);
    void gathering_candidate();
    std::string to_string();
    sigslot::signal2<IceTransportChannel*, const std::vector<Candidate>&> signal_candidate_allocate_done;
private:
    void _on_unknown_address(xrtc::UDPPort *port, const rtc::SocketAddress &addr, xrtc::StunMessage *msg,
                             const std::string &remote_ufrag);
private:

    EventLoop* _el;
    std::string _transport_name;
    IceCandidateComponent _component;
    PortAllocator* _allocator;
    IceParameters _ice_params;
    IceParameters _remote_ice_params;
    std::vector<Candidate> _local_candidates;

};
}


#endif //XRTCSERVER_ICE_TRANSPORT_CHANNEL_H
