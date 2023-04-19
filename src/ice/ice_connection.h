//
// Created by faker on 23-4-19.
//

#ifndef XRTCSERVER_ICE_CONNECTION_H
#define XRTCSERVER_ICE_CONNECTION_H

#include "base/event_loop.h"
#include "udp_port.h"
#include "candidate.h"

namespace xrtc {
    class UDPPort;
    class IceConnection {
    public:
        IceConnection(EventLoop *el, UDPPort *port, const Candidate &remote_candidate);

        ~IceConnection();
        const Candidate& remote_candidate() const { return _remote_candidate; }
    private:
        EventLoop* _el;
        UDPPort* _port;
        Candidate _remote_candidate;
    };
}


#endif //XRTCSERVER_ICE_CONNECTION_H
