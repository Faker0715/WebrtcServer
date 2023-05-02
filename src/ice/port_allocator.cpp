

#include "ice/port_allocator.h"

namespace xrtc {

PortAllocator::PortAllocator() :
    _network_manager(new NetworkManager())
{
    _network_manager->create_networks();
}

PortAllocator::~PortAllocator() = default;

const std::vector<Network*>& PortAllocator::get_networks() {
    return _network_manager->get_networks();
}

void PortAllocator::set_port_range(int min_port, int max_port) {
    if (min_port > 0) {
        _min_port = min_port;
    }

    if (max_port > 0) {
        _max_port = max_port;
    }
}

} // namespace xrtc


