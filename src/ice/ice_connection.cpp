#include "ice/ice_connection.h"

#include <rtc_base/logging.h>
#include <rtc_base/time_utils.h>
#include <rtc_base/helpers.h>

#include "ice/udp_port.h"

namespace xrtc {

// old_rtt : new_rtt = 3 : 1
const int RTT_RATIO = 3;
const int MIN_RTT = 100;
const int MAX_RTT = 60000;

ConnectionRequest::ConnectionRequest(IceConnection* conn) :
    StunRequest(new StunMessage()), connection_(conn)
{
}

void ConnectionRequest::Prepare(StunMessage* msg) {
    msg->set_type(STUN_BINDING_REQUEST);
    std::string username;
    connection_->port()->CreateStunUsername(
            connection_->remote_candidate().username, &username);
    msg->AddAttribute(std::make_unique<StunByteStringAttribute>(
                STUN_ATTR_USERNAME, username));
    msg->AddAttribute(std::make_unique<StunUInt64Attribute>(
                STUN_ATTR_ICE_CONTROLLING, 0));
    msg->AddAttribute(std::make_unique<StunByteStringAttribute>(
                STUN_ATTR_USE_CANDIDATE, 0));
    // priority
    int type_pref = ICE_TYPE_PREFERENCE_PRFLX;
    uint32_t prflx_priority = (type_pref << 24) |
        (connection_->local_candidate().priority & 0x00FFFFFF);
    msg->AddAttribute(std::make_unique<StunUInt32Attribute>(
                STUN_ATTR_PRIORITY, prflx_priority));
    msg->AddMessageIntegrity(connection_->remote_candidate().password);
    msg->AddFingerprint();
}

void ConnectionRequest::OnRequestResponse(StunMessage* msg) {
    connection_->OnConnectionRequestResponse(this, msg);
}

void ConnectionRequest::OnRequestErrorResponse(StunMessage* msg) {
    connection_->OnConnectionRequestErrorResponse(this, msg);
}

const Candidate& IceConnection::local_candidate() const {
    return port_->candidates()[0];
}

IceConnection::IceConnection(EventLoop* el, 
        UDPPort* port, 
        const Candidate& remote_candidate) :
    el_(el),
    port_(port),
    remote_candidate_(remote_candidate)
{
    requests_.SignalSendPacket.connect(this, &IceConnection::OnStunSendPacket);
}

IceConnection::~IceConnection() {

}

void IceConnection::OnStunSendPacket(StunRequest* request, const char* buf, size_t len) {
    int ret = port_->SendTo(buf, len, remote_candidate_.address);
    if (ret < 0) {
        RTC_LOG(LS_WARNING) << ToString() << ": Failed to send STUN binding request: ret="
            << ret << ", id=" << rtc::hex_encode(request->id());
    }
}

void IceConnection::PrintPingsSinceLastResponse(std::string& pings, size_t max) {
    std::stringstream ss;
    if (pings_since_last_response_.size() > max) {
        for (size_t i = 0; i < max; ++i) {
            ss << rtc::hex_encode(pings_since_last_response_[i].id) << " ";
        }
        ss << "... " << (pings_since_last_response_.size() - max) << " more";
    } else {
        for (auto ping : pings_since_last_response_) {
            ss << rtc::hex_encode(ping.id) << " ";
        }
    }
    pings = ss.str();
}

int64_t IceConnection::last_received() {
    return std::max(std::max(last_ping_received_, last_ping_response_received_),
            last_data_received_);
}

int IceConnection::ReceivingTimeout() {
    return WEAK_CONNECTION_RECEIVE_TIMEOUT;
}

void IceConnection::UpdateReceiving(int64_t now) {
    bool receiving;
    if (last_ping_sent_ < last_ping_response_received_) {
        receiving = true;
    } else {
        receiving = last_received() > 0 && 
            (now < last_received() + ReceivingTimeout());
    }

    if (receiving_ == receiving) {
        return;
    }

    RTC_LOG(LS_INFO) << ToString() << ": set receiving to " << receiving;
    receiving_ = receiving;
    SignalStateChange(this);
}

void IceConnection::set_write_state(WriteState state) {
    WriteState old_state = write_state_;
    write_state_ = state;
    if (old_state != state) {
        RTC_LOG(LS_INFO) << ToString() << ": set write state from " << old_state
            << " to " << state;
        SignalStateChange(this);
    }
}

void IceConnection::ReceivedPingResponse(int rtt) {
    // old_rtt : new_rtt = 3 : 1
    // 5 10 20
    // rtt = 5
    // rtt = 5 * 0.75 + 10 * 0.25 = 3.75 + 2.5 = 6.25
    if (rtt_samples_ > 0) {
        rtt_ = rtc::GetNextMovingAverage(rtt_, rtt, RTT_RATIO);
    } else {
        rtt_ = rtt;
    }
    
    ++rtt_samples_;

    last_ping_response_received_ = rtc::TimeMillis();
    pings_since_last_response_.clear();
    UpdateReceiving(last_ping_response_received_);
    set_write_state(STATE_WRITABLE);
    set_state(IceCandidatePairState::SUCCEEDED);
}

void IceConnection::OnConnectionRequestResponse(ConnectionRequest* request, 
        StunMessage* msg) 
{
    int rtt = request->elapsed();
    std::string pings;
    PrintPingsSinceLastResponse(pings, 5);
    RTC_LOG(LS_INFO) << ToString() << ": Received "
        << StunMethodToString(msg->type())
        << ", id=" << rtc::hex_encode(msg->transaction_id())
        << ", rtt=" << rtt
        << ", pings=" << pings;
    ReceivedPingResponse(rtt);
}

void IceConnection::set_state(IceCandidatePairState state) {
    IceCandidatePairState old_state = state_;
    state_ = state;
    if (old_state != state) {
        RTC_LOG(LS_INFO) << ToString() << ": set_state " << old_state << "->" << state_;
    }
}

void IceConnection::FailAndDestroy() {
    set_state(IceCandidatePairState::FAILED);
    Destroy();
}

void IceConnection::Destroy() {
    RTC_LOG(LS_INFO) << ToString() << ": Connection destroyed";
    SignalConnectionDestroy(this);
    delete this;
}

bool IceConnection::TooManyPingFails(size_t max_pings, int rtt, int64_t now) {
    if (pings_since_last_response_.size() < max_pings) {
        return false;
    }

    int expected_response_time = pings_since_last_response_[max_pings-1].sent_time + rtt;
    return now > expected_response_time;
}

bool IceConnection::TooLongWithoutResponse(int min_time, int64_t now) {
    if (pings_since_last_response_.empty()) {
        return false;
    }

    return now > pings_since_last_response_[0].sent_time + min_time;
}

void IceConnection::UpdateState(int64_t now) {
    int rtt = 2 * rtt_;
    if (rtt < MIN_RTT) {
        rtt = MIN_RTT;
    } else if (rtt > MAX_RTT) {
        rtt = MAX_RTT;
    }

    if (write_state_ == STATE_WRITABLE &&
            TooManyPingFails(CONNECTION_WRITE_CONNECT_FAILS, rtt, now) &&
            TooLongWithoutResponse(CONNECTION_WRITE_CONNECT_TIMEOUT, now))
    {
        RTC_LOG(LS_INFO) << ToString() << ": Unwritable after "
            << CONNECTION_WRITE_CONNECT_FAILS << " ping fails and "
            << now - pings_since_last_response_[0].sent_time
            << "ms without a response";

        set_write_state(STATE_WRITE_UNRELIABLE);
    }

    if ((write_state_ == STATE_WRITE_UNRELIABLE || write_state_ == STATE_WRITE_INIT) &&
            TooLongWithoutResponse(CONNECTION_WRITE_TIMEOUT, now))
    {
        RTC_LOG(LS_INFO) << ToString() << ": Timeout after "
            << now - pings_since_last_response_[0].sent_time
            << "ms without a response";

        set_write_state(STATE_WRITE_TIMEOUT);
    }

    UpdateReceiving(now);
}

void IceConnection::OnConnectionRequestErrorResponse(ConnectionRequest* request, 
        StunMessage* msg) 
{
    int rtt = request->elapsed();
    int error_code = msg->GetErrorCodeValue();
    RTC_LOG(LS_WARNING) << ToString() << ": Received: "
        << StunMethodToString(msg->type())
        << ", id=" << rtc::hex_encode(msg->transaction_id())
        << ", rtt=" << rtt
        << ", code=" << error_code;

    if (STUN_ERROR_UNAUTHORIZED == error_code ||
            STUN_ERROR_UNKNOWN_ATTRIBUTE == error_code ||
            STUN_ERROR_SERVER_ERROR == error_code)
    {
        // retry maybe recover
    } else {
        FailAndDestroy();
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

void IceConnection::HandleStunBindingRequest(StunMessage* stun_msg) {
    // role的冲突问题
    
    // 发送binding response
    SendStunBindingResponse(stun_msg);
}

void IceConnection::SendStunBindingResponse(StunMessage* stun_msg) {
    const StunByteStringAttribute* username_attr = stun_msg->GetByteString(
            STUN_ATTR_USERNAME);
    if (!username_attr) {
        RTC_LOG(LS_WARNING) << "send stun binding response error: no username";
        return;
    }

    StunMessage response;
    response.set_type(STUN_BINDING_RESPONSE);
    response.set_transaction_id(stun_msg->transaction_id());
    // 4 + 8
    response.AddAttribute(std::make_unique<StunXorAddressAttribute>
            (STUN_ATTR_XOR_MAPPED_ADDRESS, remote_candidate().address));
    // 4 + 20
    response.AddMessageIntegrity(port_->ice_pwd());
    // 4 + 4
    response.AddFingerprint();

    SendResponseMessage(response);
}

void IceConnection::SendResponseMessage(const StunMessage& response) {
    const rtc::SocketAddress& addr = remote_candidate_.address;

    rtc::ByteBufferWriter buf;
    if (!response.Write(&buf)) {
        return;
    }

    int ret = port_->SendTo(buf.Data(), buf.Length(), addr);
    if (ret < 0) {
        RTC_LOG(LS_WARNING) << ToString() << ": send "
            << StunMethodToString(response.type())
            << " error, to=" << addr.ToString()
            << ", id=" << rtc::hex_encode(response.transaction_id());
        return;
    }

    RTC_LOG(LS_INFO) << ToString() << ": sent "
        << StunMethodToString(response.type())
        << " to=" << addr.ToString()
        << ", id=" << rtc::hex_encode(response.transaction_id());
}

void IceConnection::OnReadPacket(const char* buf, size_t len, int64_t ts) {
    std::unique_ptr<StunMessage> stun_msg;
    std::string remote_ufrag;
    const Candidate& remote = remote_candidate_;
    if (!port_->GetStunMessage(buf, len, remote.address, &stun_msg, &remote_ufrag)) {
        // 这个不是stun包，可能是其它的比如dtls或者rtp包
        SignalReadPacket(this, buf, len, ts); 
    } else if (!stun_msg) {
    } else { // stun message
        switch (stun_msg->type()) {
            case STUN_BINDING_REQUEST:
                if (remote_ufrag != remote.username) {
                    RTC_LOG(LS_WARNING) << ToString() << ": Received "
                        << StunMethodToString(stun_msg->type())
                        << " with bad username=" << remote_ufrag
                        << ", id=" << rtc::hex_encode(stun_msg->transaction_id());
                    port_->SendBindingErrorResponse(stun_msg.get(),
                            remote.address, STUN_ERROR_UNAUTHORIZED,
                            STUN_ERROR_REASON_UNAUTHORIZED);
                } else {
                    RTC_LOG(LS_INFO) << ToString() << ": Received "
                        << StunMethodToString(stun_msg->type())
                        << ", id=" << rtc::hex_encode(stun_msg->transaction_id());
                    HandleStunBindingRequest(stun_msg.get());
                }
                break;
            case STUN_BINDING_RESPONSE:
            case STUN_BINDING_ERROR_RESPONSE:
                stun_msg->ValidateMessageIntegrity(remote_candidate_.password);
                if (stun_msg->IntegrityOk()) {
                    requests_.CheckResponse(stun_msg.get());
                }
                break;
            default:
                break;
        }
    }
}

void IceConnection::MaybeSetRemoteIceParams(const IceParameters& ice_params) {
    if (remote_candidate_.username == ice_params.ice_ufrag &&
            remote_candidate_.password.empty())
    {
        remote_candidate_.password = ice_params.ice_pwd;
    }
}

bool IceConnection::stable(int64_t now) const {
    return rtt_samples_ > RTT_RATIO + 1 && !MissResponse(now);
}

bool IceConnection::MissResponse(int64_t now) const {
    if (pings_since_last_response_.empty()) {
        return false;
    }

    int waiting = now - pings_since_last_response_[0].sent_time;
    return waiting > 2 * rtt_;
}

void IceConnection::Ping(int64_t now) {
    last_ping_sent_ = now;

    ConnectionRequest* request = new ConnectionRequest(this);
    pings_since_last_response_.push_back(SentPing(request->id(), now));
    RTC_LOG(LS_INFO) << ToString() << ": Sending STUN ping, id=" 
        << rtc::hex_encode(request->id());
    requests_.Send(request);
    set_state(IceCandidatePairState::IN_PROGRESS);
    num_pings_sent_++;
}

int IceConnection::SendPacket(const char* data, size_t len) {
    if (!port_) {
        return -1;
    }

    return port_->SendTo(data, len, remote_candidate_.address);
}

std::string IceConnection::ToString() {
    std::stringstream ss;
    ss << "Conn[" << this << ":" << port_->transport_name() 
        << ":" << port_->component() 
        << ":" << port_->local_addr().ToString()
        << "->" << remote_candidate_.address.ToString();
    return ss.str();
}

} // namespace xrtc


