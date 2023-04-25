//
// Created by faker on 23-4-19.
//

#include "ice_connection.h"
#include "rtc_base/logging.h"
#include "rtc_base/time_utils.h"
#include "rtc_base/helpers.h"

namespace xrtc {
    const int RTT_RATIO = 3;
    const int MIN_RTT = 100;
    const int MAX_RTT = 60000;

    IceConnection::IceConnection(EventLoop *el, UDPPort *port, const Candidate &remote_candidate) : _el(el),
                                                                                                    _port(port),
                                                                                                    _remote_candidate(
                                                                                                            remote_candidate) {
        _requests.signal_send_packet.connect(this, &IceConnection::_on_stun_send_packet);

    }

    IceConnection::~IceConnection() {

    }

    void IceConnection::on_read_packet(const char *buf, size_t len, int64_t ts) {

        std::unique_ptr<StunMessage> stun_msg;
        std::string remote_ufrag;
        const Candidate &remote = _remote_candidate;
        if (!_port->get_stun_message(buf, len, remote.address, &stun_msg, &remote_ufrag)) {

            // 不是stun包，可能是其他的比如dtls或者rtp包
            signal_read_packet(this,buf,len,ts);

        } else if (!stun_msg) {

        } else {
            switch (stun_msg->type()) {
                case STUN_BINDING_REQUEST:
                    if (remote_ufrag != remote.username) {
                        RTC_LOG(LS_WARNING) << to_string() << ": Receivced "
                                            << stun_method_to_string(stun_msg->type())
                                            << " with bad username=" << remote_ufrag
                                            << " id=" << rtc::hex_encode(stun_msg->transaction_id());
                        _port->send_binding_error_response(stun_msg.get(), remote.address, STUN_ERROR_UNAUTHORIZED,
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
        response.add_attribute(
                std::make_unique<StunXorAddressAttribute>(STUN_ATTR_XOR_MAPPED_ADDRESS, remote_candidate().address));
        response.add_message_integrity(_port->ice_pwd());
        response.add_fingerprint();

        send_response_message(response);


    }

    void IceConnection::send_response_message(const StunMessage &response) {
        const rtc::SocketAddress &addr = _remote_candidate.address;
        rtc::ByteBufferWriter buf;
        if (!response.write(&buf)) {
            return;
        }
        int ret = _port->send_to(buf.Data(), buf.Length(), addr);
        if (ret < 0) {
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


    std::string IceConnection::to_string() {
        std::stringstream ss;
        ss << "Conn[" << this << ":" << _port->transport_name()
           << ":" << _port->component()
           << ":" << _port->local_addr().ToString()
           << "->" << _remote_candidate.address.ToString();
        return ss.str();
    }

    void IceConnection::maybe_set_remote_ice_params(const IceParameters &ice_params) {
        if (_remote_candidate.username == ice_params.ice_ufrag && _remote_candidate.password.empty()) {
            _remote_candidate.password = ice_params.ice_pwd;
        }

    }

    bool IceConnection::stable(int64_t now) const {
        return _rtt_samples > RTT_RATIO + 1 && !_miss_response(now);
    }

    bool IceConnection::_miss_response(int64_t now) const{
        if(_pings_since_last_response.empty()){
            return false;
        }
        int waiting = now - _pings_since_last_response[0].sent_time;

        return waiting > 2 * _rtt;

    }
    void IceConnection::ping(int64_t now) {
        _last_ping_sent = now;
        ConnectionRequest *request = new ConnectionRequest(this);
        _pings_since_last_response.push_back(SentPing(request->id(), now));
        RTC_LOG(LS_INFO) << to_string() << ": Sending STUN ping, id="
                         << rtc::hex_encode(request->id());
        _requests.send(request);
        set_state(IceCandidatePairState::IN_PROGRESS);
        _num_pings_sent++;
    }

    const Candidate &IceConnection::local_candidate() const {
        return _port->candidates()[0];
    }

    void IceConnection::_on_stun_send_packet(StunRequest *request, const char *buf, size_t len) {
        int ret = _port->send_to(buf, len, _remote_candidate.address);
        if (ret < 0) {
            RTC_LOG(LS_WARNING) << to_string() << ": Failed to send binding request: ret= "
                                << ret << ", id=" << rtc::hex_encode(request->id());
        }

    }

    int64_t IceConnection::last_received() {
        return std::max(std::max(_last_ping_received, _last_ping_response_received), _last_data_received);
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
                    // 在超时范围之内
                        (now < last_received() + receiving_timeout());
        }

        if (_receiving == receiving) {
            return;
        }
        RTC_LOG(LS_INFO) << to_string() << ": set receiving to " << receiving;
        _receiving = receiving;
        //发送一个通知 以便于ice_transportchannel感知状态的变化
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
        // rtt传入的是某一个时刻的rtt

        // 平滑算法
        //old_rtt : new_rtt = 3 : 1
        // 5 10 20
        // 当10到来了 rtt = 5*0.75 + 10 * 0.25 = 6.25

        if (_rtt_samples > 0) {
            // 至少收到了一个ping的response
            _rtt = rtc::GetNextMovingAverage(_rtt, rtt, RTT_RATIO);
        } else {
            _rtt = rtt;
        }
        ++_rtt_samples;
        _last_ping_response_received = rtc::TimeMillis();
        // 一旦收到pingreponse 就把缓存清除掉
        _pings_since_last_response.clear();
        // 收到任何数据都会调用update_receiving方法
        update_receiving(_last_ping_response_received);
        // 一旦ping收到ping response 就是可写入状态
        set_write_state(WriteState::STATE_WRITABLE);
        set_state(IceCandidatePairState::SUCCEEDED);

    }

    void IceConnection::on_connection_request_response(ConnectionRequest *request, StunMessage *msg) {
        // 往返延迟
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

    void IceConnection::fail_and_destory() {
        set_state(IceCandidatePairState::FAILED);
        destroy();
    }
    void IceConnection::destroy(){
        RTC_LOG(LS_INFO) << to_string() << ": Connection destroyed";
        signal_connection_destroy(this);
        delete this;
    }

    void IceConnection::on_connection_error_request_response(ConnectionRequest *request, StunMessage *msg) {
        int rtt = request->elapsed();
        int error_code = msg->get_error_code_value();
        RTC_LOG(LS_WARNING) << to_string() << ": Received: " << stun_method_to_string(msg->type())
                            << ", id=" << rtc::hex_encode(msg->transaction_id())
                            << ", rtt=" << rtt
                            << ", code=" << error_code;
        if (STUN_ERROR_UNAUTHORIZED == error_code ||
            STUN_ERROR_UNKNOWN_ATTRIBUTE == error_code ||
            STUN_ERROR_SERVER_ERROR == error_code) {
            // retry maybe recover
        } else {
            fail_and_destory();
        }

    }

    void IceConnection::print_pings_since_last_response(std::string &pings, size_t max) {
        std::stringstream ss;
        if (_pings_since_last_response.size() > max) {
            for (size_t i = 0; i < max; ++i) {
                ss << rtc::hex_encode(_pings_since_last_response[i].id) << " ";
            }
            ss << "... " << (_pings_since_last_response.size() - max) << " more";
        } else {
            for (auto ping: _pings_since_last_response) {
                ss << rtc::hex_encode(ping.id) << " ";
            }
        }
        pings = ss.str();

    }

    uint64_t IceConnection::priority() {
        // rtc5245
        // g: controlling candidate priority
        // d: controlled candidate priority
        // conn priority = 2^32 * MIN(g,d) + 2 * MAX(g,d) + (g > d ? 1 : 0)
        uint32_t g = local_candidate().priority;
        uint32_t d = remote_candidate().priority;
        uint64_t priority = std::min(g, d);
        priority = priority << 32;
        return priority + 2 * std::max(g, d) + (g > d ? 1 : 0);
    }

    void IceConnection::set_state(IceCandidatePairState state) {
        IceCandidatePairState old_state = _state;
        _state = state;
        if (old_state != state) {
            RTC_LOG(LS_INFO) << to_string() << ": set state" << old_state << "->" << _state;
        }

    }


    // 判断一定次数后 没有收到任何的响应
    bool IceConnection::_too_many_ping_fails(size_t max_pings,int rtt, int64_t now){
        if(_pings_since_last_response.size() < max_pings){
            return false;
        }
        int expected_response_time = _pings_since_last_response[max_pings - 1].sent_time + rtt;
        return now > expected_response_time;
    }
    bool IceConnection::_too_long_without_response(int min_time,int64_t now){
        if(_pings_since_last_response.empty()){
            return false;
        }
        return now > _pings_since_last_response[0].sent_time + min_time;
    }



    void IceConnection::update_state(int64_t now) {
        int rtt = 2* _rtt;
        if(rtt < MIN_RTT){
            rtt = MIN_RTT;
        }else if(rtt > MAX_RTT){
           rtt = MAX_RTT;
        }
        if(_write_state == STATE_WRITABLE && _too_many_ping_fails(CONNECTION_WRITE_CONNECT_FAILS,rtt,now)
        && _too_long_without_response(CONNECTION_WRITE_CONNECT_TIMEOUT,now)){
            RTC_LOG(LS_INFO) << to_string() << ": Unwritable after " << CONNECTION_WRITE_CONNECT_FAILS
                << " ping fails and " << now - _pings_since_last_response[0].sent_time
                << "ms without a response";
            set_write_state(STATE_WRITE_UNRELIABLE);
        }
        if((_write_state == STATE_WRITE_UNRELIABLE || _write_state == STATE_WRITE_INIT) &&
                _too_long_without_response(CONNECTION_WRITE_TIMEOUT,now)){
            RTC_LOG(LS_INFO) << to_string() << ": Timeout after " << now - _pings_since_last_response[0].sent_time
                             << "ms without a response";
            set_write_state(STATE_WRITE_TIMEOUT);
        }
        update_receiving(now);

    }

    ConnectionRequest::ConnectionRequest(IceConnection *conn) : StunRequest(new StunMessage()), _connection(conn) {

    }

    void ConnectionRequest::prepare(StunMessage *msg) {
        msg->set_type(STUN_BINDING_REQUEST);
        std::string username;
        _connection->port()->create_stun_username(_connection->remote_candidate().username, &username);
        msg->add_attribute(std::make_unique<StunByteStringAttribute>(STUN_ATTR_USERNAME, username));
        //  不涉及角色冲突问题 就设置成0就可以了 否则就要随机一个数

        msg->add_attribute(std::make_unique<StunUInt64Attribute>(STUN_ATTR_ICE_CONTROLLING, 0));
        msg->add_attribute(std::make_unique<StunByteStringAttribute>(STUN_ATTR_USE_CANDIDATE, 0));

        // priority
        int type_pref = ICE_TYPE_PREFERENCE_PRFLX;
        uint32_t prflx_priority = (type_pref << 24) |
                                  (_connection->local_candidate().priority & 0x00FFFFFF);
        msg->add_attribute(std::make_unique<StunUInt32Attribute>(STUN_ATTR_PRIORITY, prflx_priority));
        msg->add_message_integrity(_connection->remote_candidate().password);
        msg->add_fingerprint();


    }

    void ConnectionRequest::on_request_response(StunMessage *msg) {
        _connection->on_connection_request_response(this, msg);
    }

    void ConnectionRequest::on_error_request_response(StunMessage *msg) {

        _connection->on_connection_error_request_response(this, msg);
    }
}


