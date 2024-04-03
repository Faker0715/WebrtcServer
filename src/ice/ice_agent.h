#ifndef  __XRTCSERVER_ICE_ICE_AGENT_H_
#define  __XRTCSERVER_ICE_ICE_AGENT_H_

#include <vector>

#include "base/event_loop.h"
#include "ice/ice_transport_channel.h"

namespace xrtc {

class IceAgent : public sigslot::has_slots<> {
public:
    IceAgent(EventLoop* el, PortAllocator* allocator);
    ~IceAgent();
    
    bool CreateChannel(EventLoop* el, const std::string& transport_name,
            IceCandidateComponent component);
    
    IceTransportChannel* GetChannel(const std::string& transport_name,
            IceCandidateComponent component);
   
    void SetIceParams(const std::string& transport_name,
            IceCandidateComponent component,
            const IceParameters& ice_params);
    void SetRemoteIceParams(const std::string& transport_name,
            IceCandidateComponent component,
            const IceParameters& ice_params);

    void GatheringCandidate();
    IceTransportState ice_state() { return ice_state_; }

    sigslot::signal4<IceAgent*, const std::string&, IceCandidateComponent,
        const std::vector<Candidate>&> SignalCandidateAllocateDone;
    sigslot::signal2<IceAgent*, IceTransportState> SignalIceState;

private:
    std::vector<IceTransportChannel*>::iterator GetChannelIter(
            const std::string& transport_name,
            IceCandidateComponent component);
    
    void OnCandidateAllocateDone(IceTransportChannel* channel,
            const std::vector<Candidate>&);
    void OnIceReceivingState(IceTransportChannel* channel);
    void OnIceWritableState(IceTransportChannel* channel);
    void OnIceStateChanged(IceTransportChannel* channel);
    void UpdateState();

private:
    EventLoop* el_;
    std::vector<IceTransportChannel*> channels_;
    PortAllocator* allocator_;
    IceTransportState ice_state_ = IceTransportState::kNew;
};

} // namespace xrtc

#endif  //__XRTCSERVER_ICE_ICE_AGENT_H_


