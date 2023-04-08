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
    class TransportController{
    public:
        TransportController(EventLoop* el,PortAllocator* allocator);
        ~TransportController();
        int set_local_description(SessionDescription* desc);
    private:
        EventLoop* _el;
        IceAgent* _ice_agent;
    };
}


#endif //XRTCSERVER_TRANSPORT_CONTROLLER_H
