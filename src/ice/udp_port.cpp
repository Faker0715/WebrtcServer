#include "ice/udp_port.h"

#include <sstream>

#include <rtc_base/logging.h>
#include <rtc_base/crc32.h>
#include <rtc_base/string_encode.h>

#include "base/socket.h"
#include "ice/ice_connection.h"

namespace xrtc {

UDPPort::UDPPort(EventLoop* el,
        const std::string& transport_name,
        IceCandidateComponent component,
        IceParameters ice_params) :
    el_(el),
    transport_name_(transport_name),
    component_(component),
    ice_params_(ice_params)
{
}

UDPPort::~UDPPort() {
}

std::string ComputeFoundation(const std::string& type,
        const std::string& protocol,
        const std::string& relay_protocol,
        const rtc::SocketAddress& base)
{
    std::stringstream ss;
    ss << type << base.HostAsURIString() << protocol << relay_protocol;
    return std::to_string(rtc::ComputeCrc32(ss.str()));
}

int UDPPort::CreateIceCandidate(Network* network, int min_port, int max_port,
        Candidate& c) 
{
    socket_ = CreateUdpSocket(network->ip().family());
    if (socket_ < 0) {
        return -1;
    }
    
    if (SockSetnonblock(socket_) != 0) {
        return -1;
    }
    
    sockaddr_in addr_in;
    addr_in.sin_family = network->ip().family();
    addr_in.sin_addr = network->ip().ipv4_address();
    if (SockBind(socket_, (struct sockaddr*)&addr_in, sizeof(sockaddr), 
                min_port, max_port) != 0)
    {
        return -1;
    }
    
    int port = 0;
    if (SockGetAddress(socket_, nullptr, &port) != 0) {
        return -1;
    }

    local_addr_.SetIP(network->ip());
    local_addr_.SetPort(port);
    
    async_socket_ = std::make_unique<AsyncUdpSocket>(el_, socket_);
    async_socket_->SignalReadPacket.connect(this,
            &UDPPort::OnReadPacket);

    RTC_LOG(LS_INFO) << "prepared socket address: " << local_addr_.ToString();
    
    c.component = component_;
    c.protocol = "udp";
    c.address = local_addr_;
    c.port = port;
    c.priority = c.GetPriority(ICE_TYPE_PREFERENCE_HOST, 0, 0);
    c.username = ice_params_.ice_ufrag;
    c.password = ice_params_.ice_pwd;
    c.type = LOCAL_PORT_TYPE;
    c.foundation = ComputeFoundation(c.type, c.protocol, "", c.address);
    
    candidates_.push_back(c);

    return 0;
}

IceConnection* UDPPort::CreateConnection(const Candidate& remote_candidate) {
    IceConnection* conn = new IceConnection(el_, this, remote_candidate);
    auto ret = connections_.insert(
            std::make_pair(conn->remote_candidate().address, conn));
    if (ret.second == false && ret.first->second != conn) {
        RTC_LOG(LS_WARNING) << ToString() << ": create ice connection on "
            << "an existing remote address, addr: " 
            << conn->remote_candidate().address.ToString();
        ret.first->second = conn;

        //todo
    }

    return conn;
}

IceConnection* UDPPort::GetConnection(const rtc::SocketAddress& addr) {
    auto iter = connections_.find(addr);
    return iter == connections_.end() ? nullptr : iter->second;
}

int UDPPort::SendTo(const char* buf, size_t len, const rtc::SocketAddress& addr) {
    if (!async_socket_) {
        return -1;
    }

    return async_socket_->SendTo(buf, len, addr);
}

void UDPPort::OnReadPacket(AsyncUdpSocket* /*socket*/, char* buf, size_t size,
        const rtc::SocketAddress& addr, int64_t ts)
{
    if (IceConnection* conn = GetConnection(addr)) {
        conn->OnReadPacket(buf, size, ts);
        return;
    }

    std::unique_ptr<StunMessage> stun_msg;
    std::string remote_ufrag;
    bool res = GetStunMessage(buf, size, addr, &stun_msg, &remote_ufrag);
    if (!res || !stun_msg) {
        return;
    }
    
    if (STUN_BINDING_REQUEST == stun_msg->type()) {
        RTC_LOG(LS_INFO) << ToString() << ": Received "
            << StunMethodToString(stun_msg->type())
            << " id=" << rtc::hex_encode(stun_msg->transaction_id())
            << " from " << addr.ToString();
        SignalUnknownAddress(this, addr, stun_msg.get(), remote_ufrag);
    }
}

bool UDPPort::GetStunMessage(const char* data, size_t len,
        const rtc::SocketAddress& addr,
        std::unique_ptr<StunMessage>* out_msg,
        std::string* out_username)
{
    if (!StunMessage::ValidateFingerprint(data, len)) {
        return false;
    }
    
    out_username->clear();

    std::unique_ptr<StunMessage> stun_msg = std::make_unique<StunMessage>();
    rtc::ByteBufferReader buf(data, len);
    if (!stun_msg->Read(&buf) || buf.Length() != 0) {
        return false;
    }
    
    if (STUN_BINDING_REQUEST == stun_msg->type()) {
        if (!stun_msg->GetByteString(STUN_ATTR_USERNAME) ||
                !stun_msg->GetByteString(STUN_ATTR_MESSAGE_INTEGRITY))
        {
            RTC_LOG(LS_WARNING) << ToString() << ": recevied "
                << StunMethodToString(stun_msg->type())
                << " without username/M-I from "
                << addr.ToString();
            SendBindingErrorResponse(stun_msg.get(), addr, STUN_ERROR_BAD_REQUEST,
                    STUN_ERROR_REASON_BAD_REQUEST);
            return true;
        }

        std::string local_ufrag;
        std::string remote_ufrag;
        if (!ParseStunUsername(stun_msg.get(), &local_ufrag, &remote_ufrag) ||
                local_ufrag != ice_params_.ice_ufrag)
        {
            RTC_LOG(LS_WARNING) << ToString() << ": recevied "
                << StunMethodToString(stun_msg->type())
                << " with bad local_ufrag: " << local_ufrag
                << " from " << addr.ToString();
            SendBindingErrorResponse(stun_msg.get(), addr, STUN_ERROR_UNAUTHORIZED,
                    STUN_ERROR_REASON_UNAUTHORIZED);
            return true;
        }

        if (stun_msg->ValidateMessageIntegrity(ice_params_.ice_pwd) !=
                StunMessage::IntegrityStatus::kIntegrityOk)
        {
            RTC_LOG(LS_WARNING) << ToString() << ": recevied "
                << StunMethodToString(stun_msg->type())
                << " with bad M-I from "
                << addr.ToString();
            SendBindingErrorResponse(stun_msg.get(), addr, STUN_ERROR_UNAUTHORIZED,
                    STUN_ERROR_REASON_UNAUTHORIZED);
            return true;
        }

        *out_username = remote_ufrag;
    }
    
    *out_msg = std::move(stun_msg);
    return true;
}

bool UDPPort::ParseStunUsername(StunMessage* stun_msg, std::string* local_ufrag,
        std::string* remote_ufrag)
{
    local_ufrag->clear();
    remote_ufrag->clear();

    const StunByteStringAttribute* attr = stun_msg->GetByteString(STUN_ATTR_USERNAME);
    if (!attr) {
        return false;
    }

    //RFRAG:LFRAG
    std::string username = attr->GetString();
    std::vector<std::string> fields;
    rtc::split(username, ':', &fields);
    if (fields.size() != 2) {
        return false;
    }

    *local_ufrag = fields[0];
    *remote_ufrag = fields[1];
    return true;
}

std::string UDPPort::ToString() {
    std::stringstream ss;
    ss << "Port[" << this << ":" << transport_name_ << ":" << component_
        << ":" << ice_params_.ice_ufrag << ":" << ice_params_.ice_pwd
        << ":" << local_addr_.ToString() << "]";
    return ss.str();
}

void UDPPort::SendBindingErrorResponse(StunMessage* stun_msg,
        const rtc::SocketAddress& addr,
        int err_code,
        const std::string& reason)
{
     if (!async_socket_) {
        return;
    }

    StunMessage response;
    response.set_type(STUN_BINDING_ERROR_RESPONSE);
    response.set_transaction_id(stun_msg->transaction_id());
    auto error_attr = StunAttribute::CreateErrorCode();
    error_attr->set_code(err_code);
    error_attr->set_reason(reason);
    response.AddAttribute(std::move(error_attr));

    if (err_code != STUN_ERROR_BAD_REQUEST && err_code != STUN_ERROR_UNAUTHORIZED) {
        response.AddMessageIntegrity(ice_params_.ice_pwd);
    }

    response.AddFingerprint();

    rtc::ByteBufferWriter buf;
    if (!response.Write(&buf)) {
        return;
    }

    int ret = async_socket_->SendTo(buf.Data(), buf.Length(), addr);
    if (ret < 0) {
        RTC_LOG(LS_WARNING) << ToString() << " send "
            << StunMethodToString(response.type())
            << " error, ret=" << ret
            << ", to=" << addr.ToString();
    } else {
        RTC_LOG(LS_WARNING) << ToString() << " send "
            << StunMethodToString(response.type())
            << ", reason=" << reason
            << ", to=" << addr.ToString();
    }
}

void UDPPort::CreateStunUsername(const std::string& remote_username, 
        std::string* stun_attr_username)
{
    stun_attr_username->clear();
    *stun_attr_username = remote_username;
    stun_attr_username->append(":");
    stun_attr_username->append(ice_params_.ice_ufrag);
}

} // namespace xrtc


