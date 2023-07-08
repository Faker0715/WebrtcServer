#include <rtc_base/logging.h>
#include <rtc_base/time_utils.h>
#include <rtc_base/helpers.h>

#include "ice/udp_port.h"
#include "ice/ice_connection.h"

namespace xrtc {

// old_rtt : new_rtt = 3 : 1
const int RTT_RATIO = 3;
const int MIN_RTT = 100;
const int MAX_RTT = 60000;

ConnectionRequest::ConnectionRequest(IceConnection* conn) :
    StunRequest(new StunMessage()), _connection(conn)
{

}

void ConnectionRequest::prepare(StunMessage* msg) {
    msg->set_type(STUN_BINDING_REQUEST);
    std::string username;
    _connection->port()->create_stun_username(
            _connection->remote_candidate().username, &username);
    msg->add_attribute(std::make_unique<StunByteStringAttribute>(
                STUN_ATTR_USERNAME, username));
    msg->add_attribute(std::make_unique<StunUInt64Attribute>(
                STUN_ATTR_ICE_CONTROLLING, 0));
    msg->add_attribute(std::make_unique<StunByteStringAttribute>(
                STUN_ATTR_USE_CANDIDATE, 0));
    // priority
    int type_pref = ICE_TYPE_PREFERENCE_PRFLX;
    uint32_t prflx_priority = (type_pref << 24) |
        (_connection->local_candidate().priority & 0x00FFFFFF);
    msg->add_attribute(std::make_unique<StunUInt32Attribute>(
                STUN_ATTR_PRIORITY, prflx_priority));
    msg->add_message_integrity(_connection->remote_candidate().password);
    msg->add_fingerprint();
}

void ConnectionRequest::on_request_response(StunMessage* msg) {
    _connection->on_connection_request_response(this, msg);
}

void ConnectionRequest::on_request_error_response(StunMessage* msg) {
    _connection->on_connection_request_error_response(this, msg);
}

const Candidate& IceConnection::local_candidate() const {
    return _port->candidates()[0];
}

IceConnection::IceConnection(EventLoop* el, 
        UDPPort* port, 
        const Candidate& remote_candidate) :
    _el(el),
    _port(port),
    _remote_candidate(remote_candidate)
{
    _requests.signal_send_packet.connect(this, &IceConnection::_on_stun_send_packet);
}

IceConnection::~IceConnection() {

}

void IceConnection::_on_stun_send_packet(StunRequest* request, const char* buf, size_t len) {
    int ret = _port->send_to(buf, len, _remote_candidate.address);
    if (ret < 0) {
        RTC_LOG(LS_WARNING) << to_string() << ": Failed to send STUN binding request: ret="
            << ret << ", id=" << rtc::hex_encode(request->id());
    }
}

void IceConnection::print_pings_since_last_response(std::string& pings, size_t max) {
    std::stringstream ss;
    if (_pings_since_last_response.size() > max) {
        for (size_t i = 0; i < max; ++i) {
            ss << rtc::hex_encode(_pings_since_last_response[i].id) << " ";
        }
        ss << "... " << (_pings_since_last_response.size() - max) << " more";
    } else {
        for (auto ping : _pings_since_last_response) {
            ss << rtc::hex_encode(ping.id) << " ";
        }
    }
    pings = ss.str();
}

int64_t IceConnection::last_received() {
    return std::max(std::max(_last_ping_received, _last_ping_response_received),
            _last_data_received);
}

int IceConnection::receiving_timeout() {
    return WEAK_CONNECTION_RECEIVE_TIMEOUT;
}

void IceConnection::update_receiving(int64_t now) {
    bool receiving;
    if (_last_ping_sent < _last_ping_response_received) {
        receiving = true;
    } else {
        receiving = last_received() > 0 && 
            (now < last_received() + receiving_timeout());
    }

    if (_receiving == receiving) {
        return;
    }

    RTC_LOG(LS_INFO) << to_string() << ": set receiving to " << receiving;
    _receiving = receiving;
    signal_state_change(this);
}

void IceConnection::set_write_state(WriteState state) {
    WriteState old_state = _write_state;
    _write_state = state;
    if (old_state != state) {
        RTC_LOG(LS_INFO) << to_string() << ": set write state from " << old_state
            << " to " << state;
        signal_state_change(this);
    }
}

void IceConnection::received_ping_response(int rtt) {
    // old_rtt : new_rtt = 3 : 1
    // 5 10 20
    // rtt = 5
    // rtt = 5 * 0.75 + 10 * 0.25 = 3.75 + 2.5 = 6.25
    if (_rtt_samples > 0) {
        _rtt = rtc::GetNextMovingAverage(_rtt, rtt, RTT_RATIO);
    } else {
        _rtt = rtt;
    }
    
    ++_rtt_samples;

    _last_ping_response_received = rtc::TimeMillis();
    _pings_since_last_response.clear();
    update_receiving(_last_ping_response_received);
    set_write_state(STATE_WRITABLE);
    set_state(IceCandidatePairState::SUCCEEDED);
}

void IceConnection::on_connection_request_response(ConnectionRequest* request, 
        StunMessage* msg) 
{
    int rtt = request->elapsed();
    std::string pings;
    print_pings_since_last_response(pings, 5);
    RTC_LOG(LS_INFO) << to_string() << ": Received "
        << stun_method_to_string(msg->type())
        << ", id=" << rtc::hex_encode(msg->transaction_id())
        << ", rtt=" << rtt
        << ", pings=" << pings;
    received_ping_response(rtt);
}

void IceConnection::set_state(IceCandidatePairState state) {
    IceCandidatePairState old_state = _state;
    _state = state;
    if (old_state != state) {
        RTC_LOG(LS_INFO) << to_string() << ": set_state " << old_state << "->" << _state;
    }
}

void IceConnection::fail_and_destroy() {
    set_state(IceCandidatePairState::FAILED);
    destroy();
}

void IceConnection::destroy() {
    RTC_LOG(LS_INFO) << to_string() << ": Connection destroyed";
    signal_connection_destroy(this);
    delete this;
}

bool IceConnection::_too_many_ping_fails(size_t max_pings, int rtt, int64_t now) {
    if (_pings_since_last_response.size() < max_pings) {
        return false;
    }

    int expected_response_time = _pings_since_last_response[max_pings-1].sent_time + rtt;
    return now > expected_response_time;
}

bool IceConnection::_too_long_without_response(int min_time, int64_t now) {
    if (_pings_since_last_response.empty()) {
        return false;
    }

    return now > _pings_since_last_response[0].sent_time + min_time;
}

void IceConnection::update_state(int64_t now) {
    int rtt = 2 * _rtt;
    if (rtt < MIN_RTT) {
        rtt = MIN_RTT;
    } else if (rtt > MAX_RTT) {
        rtt = MAX_RTT;
    }

    if (_write_state == STATE_WRITABLE &&
            _too_many_ping_fails(CONNECTION_WRITE_CONNECT_FAILS, rtt, now) &&
            _too_long_without_response(CONNECTION_WRITE_CONNECT_TIMEOUT, now))
    {
        RTC_LOG(LS_INFO) << to_string() << ": Unwritable after "
            << CONNECTION_WRITE_CONNECT_FAILS << " ping fails and "
            << now - _pings_since_last_response[0].sent_time
            << "ms without a response";

        set_write_state(STATE_WRITE_UNRELIABLE);
    }

    if ((_write_state == STATE_WRITE_UNRELIABLE || _write_state == STATE_WRITE_INIT) &&
            _too_long_without_response(CONNECTION_WRITE_TIMEOUT, now))
    {
        RTC_LOG(LS_INFO) << to_string() << ": Timeout after "
            << now - _pings_since_last_response[0].sent_time
            << "ms without a response";

        set_write_state(STATE_WRITE_TIMEOUT);
    }

    update_receiving(now);
}

void IceConnection::on_connection_request_error_response(ConnectionRequest* request, 
        StunMessage* msg) 
{
    int rtt = request->elapsed();
    int error_code = msg->get_error_code_value();
    RTC_LOG(LS_WARNING) << to_string() << ": Received: "
        << stun_method_to_string(msg->type())
        << ", id=" << rtc::hex_encode(msg->transaction_id())
        << ", rtt=" << rtt
        << ", code=" << error_code;

    if (STUN_ERROR_UNAUTHORIZED == error_code ||
            STUN_ERROR_UNKNOWN_ATTRIBUTE == error_code ||
            STUN_ERROR_SERVER_ERROR == error_code)
    {
        // retry maybe recover
    } else {
        fail_and_destroy();
    }
}

// rfc5245
// g : controlling candidate priority
// d : controlled candidate priority
// conn priority = 2^32 * min(g, d) + 2 * max(g, d) + (g > d ? 1 : 0)
uint64_t IceConnection::priority() {
    uint32_t g = local_candidate().priority;
    uint32_t d = remote_candidate().priority;
    uint64_t priority = std::min(g, d);
    priority = priority << 32;
    return priority + 2 * std::max(g, d) + (g > d ? 1 : 0);
}

void IceConnection::handle_stun_binding_request(StunMessage* stun_msg) {
    // role的冲突问题
    
    // 发送binding response
    send_stun_binding_response(stun_msg);
}

void IceConnection::send_stun_binding_response(StunMessage* stun_msg) {
    const StunByteStringAttribute* username_attr = stun_msg->get_byte_string(
            STUN_ATTR_USERNAME);
    if (!username_attr) {
        RTC_LOG(LS_WARNING) << "send stun binding response error: no username";
        return;
    }

    StunMessage response;
    response.set_type(STUN_BINDING_RESPONSE);
    response.set_transaction_id(stun_msg->transaction_id());
    // 4 + 8
    response.add_attribute(std::make_unique<StunXorAddressAttribute>
            (STUN_ATTR_XOR_MAPPED_ADDRESS, remote_candidate().address));
    // 4 + 20
    response.add_message_integrity(_port->ice_pwd());
    // 4 + 4
    response.add_fingerprint();

    RTC_LOG(LS_WARNING) << "send_response_message";
    send_response_message(response);
}

void IceConnection::send_response_message(const StunMessage& response) {
    const rtc::SocketAddress& addr = _remote_candidate.address;

    rtc::ByteBufferWriter buf;
    if (!response.write(&buf)) {
        return;
    }

    int ret = _port->send_to(buf.Data(), buf.Length(), addr);
    if (ret < 0) {
        RTC_LOG(LS_WARNING) << to_string() << ": send "
            << stun_method_to_string(response.type())
            << " error, to=" << addr.ToString()
            << ", id=" << rtc::hex_encode(response.transaction_id());
        return;
    }

    RTC_LOG(LS_INFO) << to_string() << ": sent "
        << stun_method_to_string(response.type())
        << " to=" << addr.ToString()
        << ", id=" << rtc::hex_encode(response.transaction_id());
}

void IceConnection::on_read_packet(const char* buf, size_t len, int64_t ts) {
    std::unique_ptr<StunMessage> stun_msg;
    std::string remote_ufrag;
    const Candidate& remote = _remote_candidate;
    if (!_port->get_stun_message(buf, len, remote.address, &stun_msg, &remote_ufrag)) {
        // 这个不是stun包，可能是其它的比如dtls或者rtp包
        signal_read_packet(this, buf, len, ts); 
    } else if (!stun_msg) {
    } else { // stun message
        RTC_LOG(LS_INFO) << "stun message ";
        switch (stun_msg->type()) {
            case STUN_BINDING_REQUEST:
                if (remote_ufrag != remote.username) {
                    RTC_LOG(LS_WARNING) << to_string() << ": Received "
                        << stun_method_to_string(stun_msg->type())
                        << " with bad username=" << remote_ufrag
                        << ", id=" << rtc::hex_encode(stun_msg->transaction_id());
                    _port->send_binding_error_response(stun_msg.get(),
                            remote.address, STUN_ERROR_UNAUTHORIZED,
                            STUN_ERROR_REASON_UNAUTHORIZED);
                } else {
                    RTC_LOG(LS_INFO) << to_string() << ": Received "
                        << stun_method_to_string(stun_msg->type())
                        << ", id=" << rtc::hex_encode(stun_msg->transaction_id());
                    handle_stun_binding_request(stun_msg.get());
                }
                break;
            case STUN_BINDING_RESPONSE:
            case STUN_BINDING_ERROR_RESPONSE:
                stun_msg->validate_message_integrity(_remote_candidate.password);
                if (stun_msg->integrity_ok()) {
                    _requests.check_response(stun_msg.get());
                }
                break;
            default:
                break;
        }
    }
}

void IceConnection::maybe_set_remote_ice_params(const IceParameters& ice_params) {
    if (_remote_candidate.username == ice_params.ice_ufrag &&
            _remote_candidate.password.empty())
    {
        _remote_candidate.password = ice_params.ice_pwd;
    }
}

bool IceConnection::stable(int64_t now) const {
    return _rtt_samples > RTT_RATIO + 1 && !_miss_response(now);
}

bool IceConnection::_miss_response(int64_t now) const {
    if (_pings_since_last_response.empty()) {
        return false;
    }

    int waiting = now - _pings_since_last_response[0].sent_time;
    return waiting > 2 * _rtt;
}

void IceConnection::ping(int64_t now) {
    _last_ping_sent = now;

    ConnectionRequest* request = new ConnectionRequest(this);
    _pings_since_last_response.push_back(SentPing(request->id(), now));
    RTC_LOG(LS_INFO) << to_string() << ": Sending STUN ping, id=" 
        << rtc::hex_encode(request->id());
    _requests.send(request);
    set_state(IceCandidatePairState::IN_PROGRESS);
    _num_pings_sent++;
}

int IceConnection::send_packet(const char* data, size_t len) {
    if (!_port) {
        return -1;
    }

    return _port->send_to(data, len, _remote_candidate.address);
}

std::string IceConnection::to_string() {
    std::stringstream ss;
    ss << "Conn[" << this << ":" << _port->transport_name() 
        << ":" << _port->component() 
        << ":" << _port->local_addr().ToString()
        << "->" << _remote_candidate.address.ToString();
    return ss.str();
}

} // namespace xrtc


