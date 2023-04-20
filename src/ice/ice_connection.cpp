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

        send_response_message(response);


    }
    void IceConnection::send_response_message(const StunMessage& response) {
        const rtc::SocketAddress& addr = _remote_candidate.address;
        rtc::ByteBufferWriter buf;
        if(!response.write(&buf)){
            return;
        }
        int ret = _port->send_to(buf.Data(),buf.Length(),addr);
        if(ret < 0){
            RTC_LOG(LS_WARNING) << to_string() << ": send "
                << stun_method_to_string(response.type())
                << " error,addr=" << addr.ToString()
                << ", id=" << rtc::hex_encode(response.transaction_id());
            return;
        }
        RTC_LOG(LS_INFO) << to_string() << ": sent "
            << stun_method_to_string(response.type())
            << " success,addr=" << addr.ToString()
            << ", id=" << rtc::hex_encode(response.transaction_id());

    }


    std::string IceConnection::to_string(){
        std::stringstream ss;
        ss << "Conn[" << this << ":" << _port->transport_name()
            << ":" << _port->component()
            << ":" << _port->local_addr().ToString()
            << "->" << _remote_candidate.address.ToString();
        return ss.str();
    }

}