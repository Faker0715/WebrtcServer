//
// Created by faker on 23-4-2.
//

#include "peer_connection.h"
#include "session_description.h"
#include "ice/ice_credentials.h"
#include "rtc_base/logging.h"

namespace xrtc {
    static RtpDirection get_direction(bool send,bool recv){
        if(send & recv){
            return RtpDirection::k_send_recv;
        }else if(send && !recv){
            return RtpDirection::k_send_only;
        }else if(!send && recv){
            return RtpDirection::k_recv_only;
        }else{
            return RtpDirection::k_inactive;
        }
    }
    PeerConnection::PeerConnection(EventLoop *el,PortAllocator* allocator) : _el(el),_transport_controller(new TransportController(el,allocator))
    {
        _transport_controller->signal_candidate_allocate_done.connect(this,&PeerConnection::on_candidate_allocate_done);;

    }

    PeerConnection::~PeerConnection() {

    }

    std::string PeerConnection::create_offer(const RTCOfferAnswerOptions options) {
        if(options.dtls_on && !_certificate){
            RTC_LOG(LS_WARNING) << "certificate is null";
            return "";
        }
        _local_desc = std::make_unique<SessionDescription>(SdpType::k_offer);

        IceParameters ice_param = IceCredentials::create_random_ice_credentials();


        if (options.recv_audio) {
            auto audio = std::make_shared<AudioContentDescription>();
            audio->set_direction(get_direction(options.send_audio,options.recv_audio));
            audio->set_rtcp_mux(options.use_rtcp_mux);
            _local_desc->add_content(audio);
            _local_desc->add_transport_info(audio->mid(),ice_param,_certificate);
        }
        if (options.recv_video) {
            auto video = std::make_shared<VideoContentDescription>();
            video->set_direction(get_direction(options.send_video,options.recv_video));
            video->set_rtcp_mux(options.use_rtcp_mux);
            _local_desc->add_content(video);
            _local_desc->add_transport_info(video->mid(),ice_param,_certificate);
        }
        if(options.use_rtp_mux){
            ContentGroup offer_bundle("BUNDLE");
            for(auto& content : _local_desc->contents()){
                offer_bundle.add_content_name(content->mid());
            }
            if(!offer_bundle.contents_names().empty()){
                _local_desc->add_group(offer_bundle);
            }
        }
        _transport_controller->set_local_description(_local_desc.get());
        return _local_desc->to_string();
    }

    int PeerConnection::init(rtc::RTCCertificate *certificate) {
        _certificate = certificate;
        return 0;
    }

    void PeerConnection::on_candidate_allocate_done(TransportController *controller, const std::string &transport_name,
                                                    IceCandidateComponent component,
                                                    const std::vector<Candidate> &candidates) {
        for(auto c:candidates){
            RTC_LOG(LS_INFO) << "candidate gathered, transport_name:" << transport_name << ", " << c.to_string();
        }
        if(!_local_desc){
            return;
        }
        auto content = _local_desc->get_content(transport_name);
        content->add_candidates(candidates);
    }
}
