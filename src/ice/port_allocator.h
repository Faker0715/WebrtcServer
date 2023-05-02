



#ifndef  __PORT_ALLOCATOR_H_
#define  __PORT_ALLOCATOR_H_

#include <memory>

#include "base/network.h"

namespace xrtc {

class PortAllocator {
public:
    PortAllocator();
    ~PortAllocator();
    
    const std::vector<Network*>& get_networks();
    
    void set_port_range(int min_port, int max_port);

    int min_port() { return _min_port; }
    int max_port() { return _max_port; }

private:
    std::unique_ptr<NetworkManager> _network_manager;
    int _min_port = 0;
    int _max_port = 0;
};

} // namespace xrtc

#endif  //__PORT_ALLOCATOR_H_


