
#ifndef  __XRTCSERVER_ICE_ICE_TRANSPORT_CHANNEL_H_
#define  __XRTCSERVER_ICE_ICE_TRANSPORT_CHANNEL_H_

#include <vector>
#include <string>

#include <rtc_base/third_party/sigslot/sigslot.h>

#include "base/event_loop.h"
#include "ice/ice_def.h"
#include "ice/port_allocator.h"
#include "ice/ice_credentials.h"
#include "ice/candidate.h"
#include "ice/stun.h"
#include "ice/ice_controller.h"

namespace xrtc {

class UDPPort;

enum class IceTransportState {
    kNew,
    kChecking,
    kConnected,
    kCompleted,
    kFailed,
    kDisconnected,
    kClosed,
};

class IceTransportChannel : public sigslot::has_slots<> {
public:
    IceTransportChannel(EventLoop* el, PortAllocator* allocator,
            const std::string& transport_name,
            IceCandidateComponent component);
    virtual ~IceTransportChannel();
    
    const std::string& transport_name() { return transport_name_; }
    IceCandidateComponent component() { return component_; }
    bool writable() { return writable_; }
    bool receiving() { return receiving_; }
    IceTransportState state() { return state_; }

    void set_ice_params(const IceParameters& ice_params);
    void set_remote_ice_params(const IceParameters& ice_params);
    void GatheringCandidate();
    int SendPacket(const char* data, size_t len);

    std::string ToString();

    sigslot::signal2<IceTransportChannel*, const std::vector<Candidate>&>
        SignalCandidateAllocateDone;
    sigslot::signal1<IceTransportChannel*> SignalReceivingState;
    sigslot::signal1<IceTransportChannel*> SignalWritableState;
    sigslot::signal1<IceTransportChannel*> SignalIceStateChanged;
    sigslot::signal4<IceTransportChannel*, const char*, size_t, int64_t> SignalReadPacket;

private:
    void OnUnknownAddress(UDPPort* port,
        const rtc::SocketAddress& addr,
        StunMessage* msg,
        const std::string& remote_ufrag);
    void AddConnection(IceConnection* conn);
    void SortConnectionsAndUpdateState();
    void MaybeStartPinging();
    void OnCheckAndPing();
    void OnConnectionStateChange(IceConnection* conn);
    void OnConnectionDestroyed(IceConnection* conn);
    void OnReadPacket(IceConnection* conn, const char* buf, size_t len, int64_t ts);
    void PingConnection(IceConnection* conn);
    void MaybeSwitchSelectedConnection(IceConnection* conn);
    void SwitchSelectedConnection(IceConnection* conn);
    void UpdateConnectionStates();
    void UpdateState();
    void set_receiving(bool receiving);
    void set_writable(bool writable);
    IceTransportState ComputeIceTransportState();

    friend void IcePingCb(EventLoop* /*el*/, TimerWatcher* /*w*/, void* data);

private:
    EventLoop* el_;
    std::string transport_name_;
    IceCandidateComponent component_;
    PortAllocator* allocator_;
    IceParameters ice_params_;
    IceParameters remote_ice_params_;
    std::vector<Candidate> local_candidates_;
    std::vector<UDPPort*> ports_;
    std::unique_ptr<IceController> ice_controller_;
    bool start_pinging_ = false;
    TimerWatcher* ping_watcher_ = nullptr;
    int cur_ping_interval_ = WEAK_PING_INTERVAL;
    int64_t last_ping_sent_ms_ = 0;
    IceConnection* selected_connection_ = nullptr;
    bool receiving_ = false;
    bool writable_ = false;
    IceTransportState state_ = IceTransportState::kNew;
    bool had_connection_ = false;
    bool has_been_connection_ = false;
};

} // namespace xrtc

#endif  //__XRTCSERVER_ICE_ICE_TRANSPORT_CHANNEL_H_


