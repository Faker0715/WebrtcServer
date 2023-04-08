//
// Created by faker on 23-4-8.
//

#ifndef XRTCSERVER_CANDIDATE_H
#define XRTCSERVER_CANDIDATE_H

#include <string>
#include "ice_def.h"
#include "rtc_base/socket_address.h"

namespace xrtc{
class Candidate {
public:
    uint32_t get_priority(uint32_t type_preference,
                          int network_adapter_preference,
                            int relay_preference);
public:
    IceCandidateComponent component;
    std::string  protocol;
    rtc::SocketAddress address;
    std::string username;
    std::string password;
    std::string type;
    std::string foundation;
    int port = 0;
    uint32_t priority;
};
}


#endif //XRTCSERVER_CANDIDATE_H
