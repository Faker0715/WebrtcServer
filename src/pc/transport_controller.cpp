//
// Created by faker on 23-4-4.
//

#include "transport_controller.h"
#include "rtc_base/logging.h"
#include "dtls_transport.h"

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
            // 创建完icetransportchannel之后还要创建一个dtlstransport
            DtlsTransport* dtls = new DtlsTransport(_ice_agent->get_channel(mid,IceCandidateComponent::RTP));
            // 放入icecontroller进行管理,一个controller可能有多个dtls连接
            dtls->set_local_certificate(_local_certificate);

            dtls->signal_receiving_state.connect(this,&TransportController::_on_dtls_receiving_state);
            dtls->signal_writable_state.connect(this,&TransportController::_on_dtls_writable_state);
            dtls->signal_dtls_state.connect(this,&TransportController::_on_dtls_state);
            _ice_agent->signal_ice_state.connect(this,&TransportController::_on_ice_state);
            _add_dtls_transport(dtls);

        }
        _ice_agent->gathering_candidate();

        return 0;
    }
    void TransportController::_on_ice_state(IceAgent* ,IceTransportState){
        _update_state();
    }

    void TransportController::_on_dtls_receiving_state(DtlsTransport*){
        _update_state();
    }

    void TransportController::_on_dtls_writable_state(DtlsTransport*){
        _update_state();
    }

    void TransportController::_on_dtls_state(DtlsTransport*,DtlsTransportState){
        _update_state();
    }
    void TransportController::_update_state(){
        PeerConnectionState pc_state = PeerConnectionState::k_new;
        // 根据dtls状态和ice_transport_channel来计算pc状态
        // 根据transportchannel状态来决定ice_agent状态
        std::map<DtlsTransportState,int> dtls_state_counts;
        std::map<IceTransportState,int> ice_state_counts;

        auto iter = _dtls_transport_by_name.begin();
        for(; iter != _dtls_transport_by_name.end();++iter){
           dtls_state_counts[iter->second->dtls_state()]++;
           ice_state_counts[iter->second->ice_channel()->state()]++;
        }
        int total_connected = ice_state_counts[IceTransportState::k_connected]
                +
                dtls_state_counts[DtlsTransportState::k_connected];
        int total_dtls_connecting = dtls_state_counts[DtlsTransportState::k_connecting];
        int total_closed = ice_state_counts[IceTransportState::k_closed] +
                dtls_state_counts[DtlsTransportState::k_closed];
        int total_failed = ice_state_counts[IceTransportState::k_failed] +
                dtls_state_counts[DtlsTransportState::k_failed];
        int total_new = ice_state_counts[IceTransportState::k_new] +
                dtls_state_counts[DtlsTransportState::k_new];
        int total_transports = _dtls_transport_by_name.size() * 2;
        int total_ice_checking = ice_state_counts[IceTransportState::k_checking];
        int total_ice_disconnected = ice_state_counts[IceTransportState::k_disconnected];
        int total_ice_completed = ice_state_counts[IceTransportState::k_completed];
        if(total_failed > 0){
            pc_state = PeerConnectionState::k_failed;
        }else if(total_ice_disconnected > 0){
            pc_state = PeerConnectionState::k_disconnected;
        }
        else if(total_new + total_closed == total_transports){
            pc_state = PeerConnectionState::k_new;
        }else if(total_ice_checking + total_dtls_connecting + total_new > 0){
            pc_state = PeerConnectionState::k_connecting;
        }else if(total_connected + total_closed + total_ice_completed == total_transports) {
            pc_state = PeerConnectionState::k_connected;
        }
        if(_pc_state != pc_state){
            _pc_state = pc_state;
            signal_connection_state(this,pc_state);
        }

    }


    void TransportController::on_candidate_allocate_done(IceAgent *agent, const std::string &transport_name,
                                                         IceCandidateComponent component,
                                                         const std::vector<Candidate> &candidates) {
        signal_candidate_allocate_done(this, transport_name, component, candidates);
    }
    DtlsTransport* TransportController::_get_dtls_transport(const std::string& transport_name){
        auto iter = _dtls_transport_by_name.find(transport_name);
        if(iter != _dtls_transport_by_name.end()){
            return iter->second;
        }
        return nullptr;
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
                auto dtls = _get_dtls_transport(mid);
                if(dtls){
                    dtls->set_remote_fingerprint(td->identity_fingerprint->algorithm,
                            td->identity_fingerprint->digest.data(),
                                                 td->identity_fingerprint->digest.size());

                }
            }
        }
        return 0;
    }

    void TransportController::_add_dtls_transport(DtlsTransport *dtls) {
        auto iter = _dtls_transport_by_name.find(dtls->transport_name());
        if (iter != _dtls_transport_by_name.end()) {
            delete iter->second;
        }
        _dtls_transport_by_name[dtls->transport_name()] = dtls;
    }

    void TransportController::set_local_certificate(rtc::RTCCertificate *cert) {
        _local_certificate = cert;
    }
}
