#ifndef  __XRTCSERVER_ICE_PORT_ALLOCATOR_H_
#define  __XRTCSERVER_ICE_PORT_ALLOCATOR_H_

#include <memory>

#include "base/network.h"

namespace xrtc {

class PortAllocator {
public:
    PortAllocator();
    ~PortAllocator();
    
    const std::vector<Network*>& GetNetworks();
    
    void SetPortRange(int min_port, int max_port);

    int min_port() { return min_port_; }
    int max_port() { return max_port_; }

private:
    std::unique_ptr<NetworkManager> network_manager_;
    int min_port_ = 0;
    int max_port_ = 0;
};

} // namespace xrtc

#endif  //__XRTCSERVER_ICE_PORT_ALLOCATOR_H_


