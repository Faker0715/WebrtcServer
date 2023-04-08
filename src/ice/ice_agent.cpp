//
// Created by faker on 23-4-4.
//

#include "ice_agent.h"
#include <algorithm>

namespace xrtc {
    IceAgent::IceAgent(EventLoop *el,PortAllocator* allocator) : _el(el),_allocator(allocator) {

    }

    IceAgent::~IceAgent() {

    }
    IceTransportChannel* IceAgent::get_channel(const std::string &transport_name,
                                               xrtc::IceCandidateComponent component) {
           auto it = _get_channel(transport_name, component);
           return it != _channels.end() ? *it : nullptr;
    }
    bool IceAgent::create_channel(EventLoop *el, const std::string &transport_name, IceCandidateComponent component) {

        if(get_channel(transport_name,component)){
            return true;
        }
        auto channel = new IceTransportChannel(el,_allocator,transport_name,component);
        _channels.push_back(channel);
        return true;
    }

    std::vector<IceTransportChannel *>::iterator
    IceAgent::_get_channel(const std::string &transport_name, IceCandidateComponent component) {
        return std::find_if(_channels.begin(), _channels.end(), [transport_name,component](IceTransportChannel *channel) {
            return channel->transport_name() == transport_name && channel->component() == component;
        });
    }

    void IceAgent::gathering_candidate() {
        for(auto channel: _channels){
            channel->gathering_candidate();
        }

    }

    void IceAgent::set_ice_params(const std::string &transport_name, IceCandidateComponent component,
                                  const IceParameters ice_params) {
        auto channel = get_channel(transport_name,component);
        if(channel){
            channel->set_ice_params(ice_params);
        }
    }

}