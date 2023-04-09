//
// Created by faker on 23-4-4.
//

#ifndef XRTCSERVER_ICE_AGENT_H
#define XRTCSERVER_ICE_AGENT_H

#include <base/event_loop.h>
#include <string>
#include <vector>
#include "ice_transport_channel.h"
#include "port_allocator.h"

namespace xrtc {
    class IceAgent : public sigslot::has_slots<> {
    public:
        IceAgent(EventLoop *el, PortAllocator *allocator);

        ~IceAgent();

        bool create_channel(EventLoop *el, const std::string &transport_name, IceCandidateComponent component);

        IceTransportChannel *get_channel(const std::string &transport_name, IceCandidateComponent component);
        void gathering_candidate();

        void set_ice_params(const std::string &transport_name, IceCandidateComponent component,
                            const IceParameters ice_params);

        sigslot::signal4<IceAgent *, const std::string &, IceCandidateComponent, const std::vector<Candidate> &> signal_candidate_allocate_done;

    private:
        std::vector<IceTransportChannel *>::iterator
        _get_channel(const std::string &transport_name, IceCandidateComponent component);

        void on_candidate_allocate_done(IceTransportChannel *channel, const std::vector<Candidate> &);

    private:
        PortAllocator *_allocator;
        EventLoop *_el;
        std::vector<IceTransportChannel *> _channels;
    };
}

#endif //XRTCSERVER_ICE_AGENT_H
