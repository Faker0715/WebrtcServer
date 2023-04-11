//
// Created by faker on 23-4-4.
//

#include "ice_agent.h"
#include "rtc_base/logging.h"
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
        channel->signal_candidate_allocate_done.connect(this,&IceAgent::on_candidate_allocate_done);
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

    void IceAgent::on_candidate_allocate_done(IceTransportChannel *channel, const std::vector<Candidate>& candidates) {
        signal_candidate_allocate_done(this,channel->transport_name(),channel->component(),candidates);
    }

    void IceAgent::set_remote_ice_params(const std::string &transport_name, IceCandidateComponent component,
                                         const IceParameters ice_params) {
        auto channel = get_channel(transport_name,component);
        if(channel){
            channel->set_remote_ice_params(ice_params);
        }
    }

}