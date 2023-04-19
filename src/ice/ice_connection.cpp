//
// Created by faker on 23-4-19.
//

#include "ice_connection.h"
#include "rtc_base/logging.h"

namespace xrtc {
    IceConnection::IceConnection(EventLoop *el, UDPPort *port, const Candidate &remote_candidate) : _el(el),
                                                                                                    _port(port),
                                                                                                    _remote_candidate(
                                                                                                            remote_candidate) {


    }

    IceConnection::~IceConnection() {

    }

    void IceConnection::on_read_packet(const char *buf, size_t len, int64_t ts) {

    }

    void IceConnection::handle_stun_binding_request(StunMessage *stun_msg) {
        // role conflict

        // send binding response
        send_stun_binding_response(stun_msg);


    }

    void IceConnection::send_stun_binding_response(StunMessage *stun_msg) {
        const StunByteStringAttribute *username_attr = stun_msg->get_byte_string(STUN_ATTR_USERNAME);
        if (!username_attr) {
            RTC_LOG(LS_WARNING) << "send stun binding response failed, no username attr";
        }
        StunMessage response;
        response.set_type(STUN_BINDING_RESPONSE);
        response.set_transaction_id(stun_msg->transaction_id());
        response.add_attribute(std::make_unique<StunXorAddressAttribute>(STUN_ATTR_XOR_MAPPED_ADDRESS,remote_candidate().address));
        response.add_message_integrity(_port->ice_pwd());
        response.add_fingerprint();

    }

}