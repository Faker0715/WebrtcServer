#include <algorithm>

#include <rtc_base/logging.h>

#include "ice/ice_agent.h"

namespace xrtc {

IceAgent::IceAgent(EventLoop* el, PortAllocator* allocator) : 
    _el(el), _allocator(allocator) {}

IceAgent::~IceAgent() {
    for (auto channel : _channels) {
        delete channel;
    }

    _channels.clear();
}

IceTransportChannel* IceAgent::get_channel(const std::string& transport_name,
        IceCandidateComponent component)
{
    auto iter = _get_channel(transport_name, component);
    return iter == _channels.end() ? nullptr : *iter;
}

bool IceAgent::create_channel(EventLoop* el, const std::string& transport_name,
        IceCandidateComponent component)
{
    if (get_channel(transport_name, component)) {
        return true;
    }
    
    auto channel = new IceTransportChannel(el, _allocator, transport_name, component);
    channel->signal_candidate_allocate_done.connect(this, 
            &IceAgent::on_candidate_allocate_done);
    channel->signal_receiving_state.connect(this,
            &IceAgent::_on_ice_receiving_state);
    channel->signal_writable_state.connect(this,
            &IceAgent::_on_ice_writable_state);
    channel->signal_ice_state_changed.connect(this,
            &IceAgent::_on_ice_state_changed);

    _channels.push_back(channel);

    return true;
}

void IceAgent::on_candidate_allocate_done(IceTransportChannel* channel,
            const std::vector<Candidate>& candidates)
{
    signal_candidate_allocate_done(this, channel->transport_name(),
            channel->component(), candidates);
}

void IceAgent::_on_ice_receiving_state(IceTransportChannel*) {
    _update_state();
}

void IceAgent::_on_ice_writable_state(IceTransportChannel*) {
    _update_state();
}

void IceAgent::_on_ice_state_changed(IceTransportChannel*) {
    _update_state();
}

void IceAgent::_update_state() {
    IceTransportState ice_state = IceTransportState::k_new;
    std::map<IceTransportState, int> ice_state_counts;
    for (auto channel : _channels) {
        ice_state_counts[channel->state()]++;
    }

    int total_ice_new = ice_state_counts[IceTransportState::k_new];
    int total_ice_checking = ice_state_counts[IceTransportState::k_checking];
    int total_ice_connected = ice_state_counts[IceTransportState::k_connected];
    int total_ice_completed = ice_state_counts[IceTransportState::k_completed];
    int total_ice_failed = ice_state_counts[IceTransportState::k_failed];
    int total_ice_disconnected = ice_state_counts[IceTransportState::k_disconnected];
    int total_ice_closed = ice_state_counts[IceTransportState::k_closed];
    int total_ice = _channels.size();

    if (total_ice_failed > 0) {
        ice_state = IceTransportState::k_failed;
    } else if (total_ice_disconnected > 0) {
        ice_state = IceTransportState::k_disconnected;
    } else if (total_ice_new + total_ice_closed == total_ice) {
        ice_state = IceTransportState::k_new;
    } else if (total_ice_new + total_ice_checking > 0) {
        ice_state = IceTransportState::k_checking;
    } else if (total_ice_completed + total_ice_closed == total_ice) {
        ice_state = IceTransportState::k_completed;
    } else if (total_ice_connected + total_ice_completed + total_ice_closed == total_ice) {
        ice_state = IceTransportState::k_connected;
    }

    if (_ice_state != ice_state) {
        // 为了保证不跳过k_connected状态
        if (_ice_state == IceTransportState::k_checking 
                && ice_state == IceTransportState::k_completed)
        {
            signal_ice_state(this, IceTransportState::k_connected);
        }

        _ice_state = ice_state;
        signal_ice_state(this, _ice_state);
    }
}

std::vector<IceTransportChannel*>::iterator IceAgent::_get_channel(
        const std::string& transport_name,
        IceCandidateComponent component)
{
    return std::find_if(_channels.begin(), _channels.end(), 
            [transport_name, component](IceTransportChannel* channel) {
                return transport_name == channel->transport_name() &&
                    component == channel->component();
            });
}

void IceAgent::set_ice_params(const std::string& transport_name,
        IceCandidateComponent component,
        const IceParameters& ice_params)
{
    auto channel = get_channel(transport_name, component);
    if (channel) {
        channel->set_ice_params(ice_params);
    }
}

void IceAgent::set_remote_ice_params(const std::string& transport_name,
        IceCandidateComponent component,
        const IceParameters& ice_params)
{
    auto channel = get_channel(transport_name, component);
    if (channel) {
        channel->set_remote_ice_params(ice_params);
    }
}

void IceAgent::gathering_candidate() {
    for (auto channel : _channels) {
        channel->gathering_candidate();
    }
}

} // namespace xrtc


