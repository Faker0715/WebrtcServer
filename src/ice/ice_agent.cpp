#include "ice/ice_agent.h"

#include <algorithm>

#include <rtc_base/logging.h>

namespace xrtc {

IceAgent::IceAgent(EventLoop* el, PortAllocator* allocator) : 
    el_(el), allocator_(allocator) {}

IceAgent::~IceAgent() {
    for (auto channel : channels_) {
        delete channel;
    }

    channels_.clear();
}

IceTransportChannel* IceAgent::GetChannel(const std::string& transport_name,
        IceCandidateComponent component)
{
    auto iter = GetChannelIter(transport_name, component);
    return iter == channels_.end() ? nullptr : *iter;
}

bool IceAgent::CreateChannel(EventLoop* el, const std::string& transport_name,
        IceCandidateComponent component)
{
    if (GetChannel(transport_name, component)) {
        return true;
    }
    
    auto channel = new IceTransportChannel(el, allocator_, transport_name, component);
    channel->SignalCandidateAllocateDone.connect(this, 
            &IceAgent::OnCandidateAllocateDone);
    channel->SignalReceivingState.connect(this,
            &IceAgent::OnIceReceivingState);
    channel->SignalWritableState.connect(this,
            &IceAgent::OnIceWritableState);
    channel->SignalIceStateChanged.connect(this,
            &IceAgent::OnIceStateChanged);

    channels_.push_back(channel);

    return true;
}

void IceAgent::OnCandidateAllocateDone(IceTransportChannel* channel,
            const std::vector<Candidate>& candidates)
{
    SignalCandidateAllocateDone(this, channel->transport_name(),
            channel->component(), candidates);
}

void IceAgent::OnIceReceivingState(IceTransportChannel*) {
    UpdateState();
}

void IceAgent::OnIceWritableState(IceTransportChannel*) {
    UpdateState();
}

void IceAgent::OnIceStateChanged(IceTransportChannel*) {
    UpdateState();
}

void IceAgent::UpdateState() {
    IceTransportState ice_state = IceTransportState::kNew;
    std::map<IceTransportState, int> ice_state_counts;
    for (auto channel : channels_) {
        ice_state_counts[channel->state()]++;
    }

    int total_ice_new = ice_state_counts[IceTransportState::kNew];
    int total_ice_checking = ice_state_counts[IceTransportState::kChecking];
    int total_ice_connected = ice_state_counts[IceTransportState::kConnected];
    int total_ice_completed = ice_state_counts[IceTransportState::kCompleted];
    int total_ice_failed = ice_state_counts[IceTransportState::kFailed];
    int total_ice_disconnected = ice_state_counts[IceTransportState::kDisconnected];
    int total_ice_closed = ice_state_counts[IceTransportState::kClosed];
    int total_ice = channels_.size();

    if (total_ice_failed > 0) {
        ice_state = IceTransportState::kFailed;
    } else if (total_ice_disconnected > 0) {
        ice_state = IceTransportState::kDisconnected;
    } else if (total_ice_new + total_ice_closed == total_ice) {
        ice_state = IceTransportState::kNew;
    } else if (total_ice_new + total_ice_checking > 0) {
        ice_state = IceTransportState::kChecking;
    } else if (total_ice_completed + total_ice_closed == total_ice) {
        ice_state = IceTransportState::kCompleted;
    } else if (total_ice_connected + total_ice_completed + total_ice_closed == total_ice) {
        ice_state = IceTransportState::kConnected;
    }

    if (ice_state_ != ice_state) {
        // 为了保证不跳过k_connected状态
        if (ice_state_ == IceTransportState::kChecking 
                && ice_state == IceTransportState::kCompleted)
        {
            SignalIceState(this, IceTransportState::kConnected);
        }

        ice_state_ = ice_state;
        SignalIceState(this, ice_state_);
    }
}

std::vector<IceTransportChannel*>::iterator IceAgent::GetChannelIter(
        const std::string& transport_name,
        IceCandidateComponent component)
{
    return std::find_if(channels_.begin(), channels_.end(), 
            [transport_name, component](IceTransportChannel* channel) {
                return transport_name == channel->transport_name() &&
                    component == channel->component();
            });
}

void IceAgent::SetIceParams(const std::string& transport_name,
        IceCandidateComponent component,
        const IceParameters& ice_params)
{
    auto channel = GetChannel(transport_name, component);
    if (channel) {
        channel->set_ice_params(ice_params);
    }
}

void IceAgent::SetRemoteIceParams(const std::string& transport_name,
        IceCandidateComponent component,
        const IceParameters& ice_params)
{
    auto channel = GetChannel(transport_name, component);
    if (channel) {
        channel->set_remote_ice_params(ice_params);
    }
}

void IceAgent::GatheringCandidate() {
    for (auto channel : channels_) {
        channel->GatheringCandidate();
    }
}

} // namespace xrtc


