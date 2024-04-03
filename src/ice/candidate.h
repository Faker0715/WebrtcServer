#ifndef  __XRTCSERVER_ICE_ICE_CANDIDATE_H_
#define  __XRTCSERVER_ICE_ICE_CANDIDATE_H_

#include <string>

#include <rtc_base/socket_address.h>

#include "ice/ice_def.h"

namespace xrtc {

class Candidate {
public:
    uint32_t GetPriority(uint32_t type_preference,
            int network_adapter_preference,
            int relay_preference);
    
    std::string ToString() const;

public:
    IceCandidateComponent component;
    std::string protocol;
    rtc::SocketAddress address;
    int port = 0;
    uint32_t priority;
    std::string username;
    std::string password;
    std::string type;
    std::string foundation;
};

} // namespace xrtc

#endif  //__XRTCSERVER_ICE_ICE_CANDIDATE_H_


