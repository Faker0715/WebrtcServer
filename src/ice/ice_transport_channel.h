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

namespace xrtc{
class IceTransportChannel {
    public:
    IceTransportChannel(EventLoop* el, PortAllocator* allocator,const std::string& transport_name, IceCandidateComponent component);
    virtual ~IceTransportChannel();
    std::string transport_name() const { return _transport_name; }
    IceCandidateComponent component() const { return _component; }
    void set_ice_params(const IceParameters ice_params);
    void gathering_candidate();
    sigslot::signal2<IceTransportChannel*, const std::vector<Candidate>&> signal_candidate_allocate_done;
private:

    EventLoop* _el;
    std::string _transport_name;
    IceCandidateComponent _component;
    PortAllocator* _allocator;
    IceParameters _ice_params;
    std::vector<Candidate> _local_candidates;

};
}


#endif //XRTCSERVER_ICE_TRANSPORT_CHANNEL_H
