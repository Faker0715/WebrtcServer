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
        addr_in.sin_addr = network->ip().ipv4_address();;

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

    void UDPPort::on_read_packet(AsyncUdpSocket *socket, char *buf, size_t size, const rtc::SocketAddress &addr,
                                 int64_t ts) {
        std::unique_ptr<StunMessage> stun_msg;
        bool res = get_stun_message(buf, size, &stun_msg);
        RTC_LOG(LS_WARNING) << "====res: " << res;
    }

    bool UDPPort::get_stun_message(const char *data, size_t len, std::unique_ptr<StunMessage> *out_msg) {
        if (!StunMessage::vaildate_fingerprint(data, len)) {
            return false;
        }
        rtc::ByteBufferReader buf(data, len);
        std::unique_ptr<StunMessage> stun_msg = std::make_unique<StunMessage>();
        if (!stun_msg->read(&buf) || buf.Length() != 0) {
            return false;
        }
        if (STUN_BINDING_REQUEST == stun_msg->type()) {
            if (!stun_msg->get_byte_string(STUN_ATTR_USERNAME) ||
                !stun_msg->get_byte_string(STUN_ATTR_MESSAGE_INTEGRITY)) {
                // todo: 发送错误响应
                return true;
            }
            std::string local_ufrag;
            std::string remote_ufrag;
            if (!_parse_stun_username(stun_msg.get(), &local_ufrag, &remote_ufrag) ||
                //可能会被别人篡改
                local_ufrag != _ice_params.ice_ufrag) {
                return true;
            }
            if(stun_msg->validate_message_integrity(_ice_params.ice_pwd) != StunMessage::IntegrityStatus::k_integtity_ok){
                return true;
            }
        }

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
        RTC_LOG(LS_WARNING) << "local_urfrag: " << *local_ufrag << ", remote_ufrag: " << *remote_ufrag;
        return true;
    }

}
