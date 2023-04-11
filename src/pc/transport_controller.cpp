//
// Created by faker on 23-4-4.
//

#include "transport_controller.h"
#include "rtc_base/logging.h"

namespace xrtc {
    TransportController::TransportController(xrtc::EventLoop *el, PortAllocator *allocator) : _el(el), _ice_agent(
            new IceAgent(el, allocator)) {
        _ice_agent->signal_candidate_allocate_done.connect(this, &TransportController::on_candidate_allocate_done);
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

    void TransportController::on_candidate_allocate_done(IceAgent *agent, const std::string &transport_name,
                                                         IceCandidateComponent component,
                                                         const std::vector<Candidate> &candidates) {
        signal_candidate_allocate_done(this, transport_name, component, candidates);
    }

    int TransportController::set_remote_description(SessionDescription *desc) {
        if (!desc) {
            return -1;
        }
        for (auto content: desc->contents()) {
            std::string mid = content->mid();
            if (desc->is_bundle(mid) && mid != desc->get_first_bundle_mid()) {
                continue;
            }
            auto td = desc->get_transport_info(mid);
            if(td){
                _ice_agent->set_remote_ice_params(content->mid(),IceCandidateComponent::RTP,IceParameters(td->ice_ufrag,td->ice_pwd));
            }
        }
        return 0;
    }
}
