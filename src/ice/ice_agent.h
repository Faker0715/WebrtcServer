#ifndef  __ICE_AGENT_H_
#define  __ICE_AGENT_H_

#include <vector>

#include "base/event_loop.h"
#include "ice/ice_transport_channel.h"

namespace xrtc {

class IceAgent : public sigslot::has_slots<> {
public:
    IceAgent(EventLoop* el, PortAllocator* allocator);
    ~IceAgent();
    
    bool create_channel(EventLoop* el, const std::string& transport_name,
            IceCandidateComponent component);
    
    IceTransportChannel* get_channel(const std::string& transport_name,
            IceCandidateComponent component);
   
    void set_ice_params(const std::string& transport_name,
            IceCandidateComponent component,
            const IceParameters& ice_params);
    void set_remote_ice_params(const std::string& transport_name,
            IceCandidateComponent component,
            const IceParameters& ice_params);

    void gathering_candidate();
    IceTransportState ice_state() { return _ice_state; }

    sigslot::signal4<IceAgent*, const std::string&, IceCandidateComponent,
        const std::vector<Candidate>&> signal_candidate_allocate_done;
    sigslot::signal2<IceAgent*, IceTransportState> signal_ice_state;

private:
    std::vector<IceTransportChannel*>::iterator _get_channel(
            const std::string& transport_name,
            IceCandidateComponent component);
    
    void on_candidate_allocate_done(IceTransportChannel* channel,
            const std::vector<Candidate>&);
    void _on_ice_receiving_state(IceTransportChannel* channel);
    void _on_ice_writable_state(IceTransportChannel* channel);
    void _on_ice_state_changed(IceTransportChannel* channel);
    void _update_state();

private:
    EventLoop* _el;
    std::vector<IceTransportChannel*> _channels;
    PortAllocator* _allocator;
    IceTransportState _ice_state = IceTransportState::k_new;
};

} // namespace xrtc

#endif  //__ICE_AGENT_H_


