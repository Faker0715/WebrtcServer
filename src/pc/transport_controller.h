//
// Created by faker on 23-4-4.
//

#ifndef XRTCSERVER_TRANSPORT_CONTROLLER_H
#define XRTCSERVER_TRANSPORT_CONTROLLER_H

#include "base/event_loop.h"
#include "ice/ice_agent.h"
#include "session_description.h"
#include "ice/port_allocator.h"

namespace xrtc{
    class TransportController : public sigslot::has_slots<>{
    public:
        TransportController(EventLoop* el,PortAllocator* allocator);
        ~TransportController();
        int set_local_description(SessionDescription* desc);
        sigslot::signal4<TransportController*, const std::string &, IceCandidateComponent, const std::vector<Candidate> &> signal_candidate_allocate_done;
    private:
        void on_candidate_allocate_done(IceAgent* agent,const std::string& transport_name,IceCandidateComponent component,const std::vector<Candidate>& candidates);
    private:
        EventLoop* _el;
        IceAgent* _ice_agent;
    };
}


#endif //XRTCSERVER_TRANSPORT_CONTROLLER_H
