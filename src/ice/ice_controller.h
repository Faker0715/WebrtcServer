#ifndef  __XRTCSERVER_ICE_ICE_CONTROLLER_H_
#define  __XRTCSERVER_ICE_ICE_CONTROLLER_H_

#include <set>

#include "ice/ice_connection.h"

namespace xrtc {

class IceTransportChannel;

struct PingResult {
    PingResult(const IceConnection* conn, int ping_interval) :
        conn(conn), ping_interval(ping_interval) {}

    const IceConnection* conn = nullptr;
    int ping_interval = 0;
};

class IceController {
public:
    IceController(IceTransportChannel* ice_channel) : ice_channel_(ice_channel) {}
    ~IceController() = default;
  
    void AddConnection(IceConnection* conn);
    const std::vector<IceConnection*> connections() { return connections_; }
    bool HasPingableConnection();
    PingResult SelectConnectionToPing(int64_t last_ping_sent_ms);
    IceConnection* SortAndSwitchConnection();
    void set_selected_connection(IceConnection* conn) { selected_connection_ = conn; }
    void MarkConnectionPinged(IceConnection* conn);
    void OnConnectionDestroyed(IceConnection* conn);
    bool ReadyToSend(IceConnection* conn);

private:
    bool IsPingable(IceConnection* conn, int64_t now);
    const IceConnection* FindNextPingableConnection(int64_t last_ping_sent_ms);
    bool IsConnectionPastPingInterval(const IceConnection* conn, int64_t now);
    int GetConnectionPingInterval(const IceConnection* conn, int64_t now);
    
    bool weak() {
        return selected_connection_ == nullptr ||  selected_connection_->weak();
    }

    bool MorePingable(IceConnection* conn1, IceConnection* conn2); 
    int CompareConnections(IceConnection* a, IceConnection* b);

private:
    IceTransportChannel* ice_channel_;
    IceConnection* selected_connection_ = nullptr;
    std::vector<IceConnection*> connections_;
    std::set<IceConnection*> unpinged_connections_;
    std::set<IceConnection*> pinged_connections_;
};

} // namespace xrtc

#endif  //__XRTCSERVER_ICE_ICE_CONTROLLER_H_


