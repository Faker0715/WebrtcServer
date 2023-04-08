//
// Created by faker on 23-4-4.
//

#include "transport_controller.h"
#include "rtc_base/logging.h"

namespace xrtc {
    TransportController::TransportController(xrtc::EventLoop *el, PortAllocator *allocator) : _el(el), _ice_agent(
            new IceAgent(el, allocator)) {
    }

    TransportController::~TransportController() {

    }

    int TransportController::set_local_description(SessionDescription *desc) {
        if (!desc) {
            RTC_LOG(LS_WARNING) << "desc is null";
            return -1;
        }
        for (auto content: desc->contents()) {
            std::string mid = content->mid();
            if (desc->is_bundle(mid) && mid != desc->get_first_bundle_mid()) {
                continue;
            }
            _ice_agent->create_channel(_el, mid, IceCandidateComponent::RTP);
            // 这里要放到后面 因为set_ice_params会查找channel
            auto td = desc->get_transport_info(mid);
            if (td) {
                _ice_agent->set_ice_params(mid, IceCandidateComponent::RTP, IceParameters(td->ice_ufrag,td->ice_pwd));
            }

        }
        _ice_agent->gathering_candidate();

        return 0;
    }
}
