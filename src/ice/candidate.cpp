//
// Created by faker on 23-4-8.
//

#include <sstream>
#include "candidate.h"
namespace xrtc{

    uint32_t Candidate::get_priority(uint32_t type_preference, int network_adapter_preference, int relay_preference) {
        int addr_ref = rtc::IPAddressPrecedence(address.ipaddr());
        int local_pref = ((network_adapter_preference << 8) | addr_ref) + relay_preference;
        return (type_preference << 24) | (local_pref << 8) | (256 - (int)component);
    }

    std::string Candidate::to_string() const {
        std::stringstream ss;
        ss << "Cand[" <<
            foundation << ":" << component << ":" <<
            protocol << ":" << priority << ":" <<
            address.ToString() << ":" << type << ":" <<
            username << ":" << password << "]";
        return ss.str();
    }
}