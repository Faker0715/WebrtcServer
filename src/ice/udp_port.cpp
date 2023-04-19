//
// Created by faker on 23-4-8.
//
#include <sstream>
#include "udp_port.h"
#include "rtc_base/logging.h"
#include "base/socket.h"
#include <rtc_base/crc32.h>
#include <rtc_base/buffer.h>
#include <rtc_base/string_encode.h>

namespace xrtc {

    UDPPort::UDPPort(xrtc::EventLoop *el, const std::string &transport_name, xrtc::IceCandidateComponent component,
                     xrtc::IceParameters ice_params) : _el(el), _transport_name(transport_name), _component(component),
                                                       _ice_params(ice_params) {

    }

    UDPPort::~UDPPort() {

    }

    std::string compute_foundation(const std::string &type, const std::string &protocol,
                                   const std::string &relay_protocol,
                                   const rtc::SocketAddress &base) {
        std::stringstream ss;
        ss << "type" << base.HostAsURIString() << protocol << relay_protocol;
        return std::to_string(rtc::ComputeCrc32(ss.str()));
    }

    int UDPPort::create_ice_candidate(Network *network, int min_port, int max_port, Candidate &c) {
        _socket = create_udp_socket(network->ip().family());
        if (_socket < 0) {
            return -1;
        }
        if (sock_setnonblock(_socket) != 0) {
            return -1;
        }
        sockaddr_in addr_in;
        addr_in.sin_family = network->ip().family();
        addr_in.sin_addr = network->ip().ipv4_address();

        if (sock_bind(_socket, (struct sockaddr *) &addr_in, sizeof(addr_in), min_port, max_port) != 0) {
            return -1;
        }
        int port = 0;
        if (sock_get_address(_socket, nullptr, &port) != 0) {
            return -1;
        }
        _local_addr.SetIP(network->ip());
        _local_addr.SetPort(port);
        _async_socket = std::make_unique<AsyncUdpSocket>(_el, _socket);

        _async_socket->signal_read_packet.connect(this, &UDPPort::on_read_packet);

        RTC_LOG(LS_INFO) << "prepared socket address: " << _local_addr.ToString();
        c.component = _component;
        c.protocol = "udp";
        c.address = _local_addr;
        c.port = port;
        c.priority = c.get_priority(ICE_TYPE_PREFERENCE_HOST, 0, 0);
        c.username = _ice_params.ice_ufrag;
        c.password = _ice_params.ice_pwd;
        c.type = LOCAL_PORT_TYPE;
        c.foundation = compute_foundation(c.type, c.protocol, "", c.address);
        _candidates.push_back(c);


        return 0;
    }
    IceConnection* UDPPort::get_connection(const rtc::SocketAddress& addr){
        auto iter = _connections.find(addr);
        return iter == _connections.end() ? nullptr : iter->second;
    }
    void UDPPort::on_read_packet(AsyncUdpSocket *socket, char *buf, size_t size, const rtc::SocketAddress &addr,
                                 int64_t ts) {

        if(IceConnection * conn = get_connection(addr)) {
            conn->on_read_packet(buf,size,ts);
            return;
        }



        std::string remote_ufrag;
        std::unique_ptr<StunMessage> stun_msg;
        bool res = get_stun_message(buf, size, addr, &stun_msg, &remote_ufrag);
        if (!res || !stun_msg){
            return;
        }
        if(STUN_BINDING_REQUEST == stun_msg->type()){
            RTC_LOG(LS_INFO) << to_string() << ": Received " << stun_method_to_string(stun_msg->type())
                << " id=" << rtc::hex_encode(stun_msg->transaction_id())
                << " from " << addr.ToString();
            // 现在这个地址对于服务器是未知的
            // 可以创建peer反射的candidate port需要发送未知的通知告诉给transportchannel
            signal_unknown_address(this,addr,stun_msg.get(),remote_ufrag);
        }

    }

    bool UDPPort::get_stun_message(const char *data, size_t len,
                                   const rtc::SocketAddress &addr,
                                   std::unique_ptr<StunMessage> *out_msg,
                                   std::string *out_username) {
        if (!StunMessage::validate_fingerprint(data, len)) {
            return false;
        }
        out_username->clear();
        rtc::ByteBufferReader buf(data, len);
        std::unique_ptr<StunMessage> stun_msg = std::make_unique<StunMessage>();
        if (!stun_msg->read(&buf) || buf.Length() != 0) {
            return false;
        }
        if (STUN_BINDING_REQUEST == stun_msg->type()) {
            if (!stun_msg->get_byte_string(STUN_ATTR_USERNAME) ||
                !stun_msg->get_byte_string(STUN_ATTR_MESSAGE_INTEGRITY)) {
                RTC_LOG(LS_WARNING) << to_string() << ": recevied " << stun_method_to_string(stun_msg->type())
                                    << " without username or message integrity from" << addr.ToString();
                send_binding_error_response(stun_msg.get(), addr, STUN_ERROR_BAD_REQUEST,
                                            STUN_ERROR_REASON_BAD_REQUEST);
                return true;
            }
            std::string local_ufrag;
            std::string remote_ufrag;
            if (!_parse_stun_username(stun_msg.get(), &local_ufrag, &remote_ufrag) ||
                //可能会被别人篡改
                local_ufrag != _ice_params.ice_ufrag) {
                RTC_LOG(LS_WARNING) << to_string() << ": recevied " << stun_method_to_string(stun_msg->type())
                                    << " with bad local_ufrag " << local_ufrag << " from " << addr.ToString();
                send_binding_error_response(stun_msg.get(), addr, STUN_ERROR_UNAUTHORIZED,
                                            STUN_ERROR_REASON_UNAUTHORIZED);
                return true;
            }
            if (stun_msg->validate_message_integrity(_ice_params.ice_pwd) !=
                StunMessage::IntegrityStatus::k_integrity_ok) {
                RTC_LOG(LS_WARNING) << to_string() << ": recevied " << stun_method_to_string(stun_msg->type())
                                    << " with bad M-I from " << addr.ToString();
                send_binding_error_response(stun_msg.get(), addr, STUN_ERROR_UNAUTHORIZED,
                                            STUN_ERROR_REASON_UNAUTHORIZED);
                return true;
            }
            *out_username = remote_ufrag;
        }

        *out_msg = std::move(stun_msg);
        return true;
    }

    bool UDPPort::_parse_stun_username(StunMessage *stun_msg, std::string *local_ufrag, std::string *remote_ufrag) {
        local_ufrag->clear();
        remote_ufrag->clear();
        const StunByteStringAttribute *attr = stun_msg->get_byte_string(STUN_ATTR_USERNAME);
        if (!attr) {
            return false;
        }
        // RFRAG:LFRAG
        std::string username = attr->get_string();
        std::vector<std::string> fields;
        rtc::split(username, ':', &fields);
        if (fields.size() != 2) {
            return false;
        }
        *local_ufrag = fields[0];
        *remote_ufrag = fields[1];
        return true;
    }

    std::string UDPPort::to_string() {
        std::stringstream ss;
        ss << "Port[" << this << ":" << _transport_name << ":" << _component
           << ":" << _ice_params.ice_ufrag << ":" << _ice_params.ice_pwd << ":" << _local_addr.ToString() << "]";
        return ss.str();
    }

    void UDPPort::send_binding_error_response(StunMessage *stun_msg, const rtc::SocketAddress &addr,
                                              int err_code, const std::string &reason) {

    }

    IceConnection *UDPPort::create_connection(const Candidate &remote_candidate) {
         IceConnection* conn = new IceConnection(_el, this, remote_candidate);
         auto ret = _connections.insert(std::make_pair(conn->remote_candidate().address,conn));

         if(!ret.second && ret.first->second != conn){
             RTC_LOG(LS_WARNING) << to_string() << ": create connection failed on "
              << "an existing remote address, adr: " << conn->remote_candidate().address.ToString();
              ret.first->second = conn;
              // todo

         }
         return conn;
    }


}
