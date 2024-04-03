#include "pc/dtls_srtp_transport.h"

#include <rtc_base/logging.h>

#include "modules/rtp_rtcp/rtp_utils.h"
#include "pc/dtls_transport.h"

namespace xrtc {

// rfc5764
static char kDtlsSrtpExporterLabel[] = "EXTRACTOR-dtls_srtp";

DtlsSrtpTransport::DtlsSrtpTransport(const std::string& transport_name,
        bool rtcp_mux_enabled) :
    SrtpTransport(rtcp_mux_enabled), transport_name_(transport_name)
{

}

void DtlsSrtpTransport::set_dtls_transports(DtlsTransport* rtp_dtls_transport,
        DtlsTransport* rtcp_dtls_transport)
{
    rtp_dtls_transport_ = rtp_dtls_transport;
    rtcp_dtls_transport_ = rtcp_dtls_transport;
    
    if (rtp_dtls_transport_) {
        rtp_dtls_transport_->SignalDtlsState.connect(this,
                &DtlsSrtpTransport::OnDtlsState);
        rtp_dtls_transport_->SignalReadPacket.connect(this,
                &DtlsSrtpTransport::OnReadPacket);
    }

    MaybeSetupDtlsSrtp();
}

void DtlsSrtpTransport::OnDtlsState(DtlsTransport* /*dtls*/,
        DtlsTransportState state)
{
    if (state != DtlsTransportState::kConnected) {
        ResetParams();
        return;
    }

    MaybeSetupDtlsSrtp();
}

void DtlsSrtpTransport::OnReadPacket(DtlsTransport* /*dtls*/,
        const char* data, size_t len, int64_t ts)
{
    auto array_view = rtc::MakeArrayView(data, len);
    RtpPacketType packet_type = InferRtpPacketType(array_view);

    if (packet_type == RtpPacketType::kUnknown) {
        return;
    }

    rtc::CopyOnWriteBuffer packet(data, len);
    if (packet_type == RtpPacketType::kRtcp) {
        OnRtcpPacketReceived(std::move(packet), ts);
    } else {
        OnRtpPacketReceived(std::move(packet), ts);
    }
}

void DtlsSrtpTransport::OnRtpPacketReceived(rtc::CopyOnWriteBuffer packet,
        int64_t ts)
{
    if (!IsSrtpActive()) {
        RTC_LOG(LS_WARNING) << "Inactive SRTP transport received a rtp packet, drop it.";
        return;
    }

    char* data = packet.MutableData<char>();
    int len = packet.size();
    if (!UnprotectRtp(data, len, &len)) {
        const int kFailLog = 100;
        if (unprotect_fail_count_ % kFailLog == 0) {
            RTC_LOG(LS_WARNING) << "Failed to unprotect rtp packet: "
                << ", size=" << len
                << ", seqnum=" << ParseRtpSequenceNumber(packet)
                << ", ssrc=" << ParseRtpSsrc(packet)
                << ", unprotect_fail_count=" << unprotect_fail_count_;
        }
        unprotect_fail_count_++;
        return;
    }

    packet.SetSize(len);
    SignalRtpPacketReceived(this, &packet, ts);
}

void DtlsSrtpTransport::OnRtcpPacketReceived(rtc::CopyOnWriteBuffer packet,
        int64_t ts)
{
    if (!IsSrtpActive()) {
        RTC_LOG(LS_WARNING) << "Inactive SRTP transport received a rtcp packet, drop it.";
        return;
    }

    char* data = packet.MutableData<char>();
    int len = packet.size();
    if (!UnprotectRtcp(data, len, &len)) {
        int type = 0;
        GetRtcpType(data, len, &type);
        RTC_LOG(LS_WARNING) << "Failed to unprotect rtcp packet: "
            << ", size=" << len
            << ", type=" << type;
        return;
    }

    packet.SetSize(len);
    SignalRtcpPacketReceived(this, &packet, ts);
}

int DtlsSrtpTransport::SendRtp(const char* buf, size_t size) {
    if (!IsSrtpActive()) {
        RTC_LOG(LS_WARNING) << "Failed to send rtp packet: Inactive srtp transport";
        return -1;
    }
   
    int rtp_auth_tag_len = 0;
    GetSendAuthTagLen(&rtp_auth_tag_len, nullptr);
    rtc::CopyOnWriteBuffer packet(buf, size, size + rtp_auth_tag_len); 
    
    char* data = (char*)packet.data();
    int len = packet.size();
    uint16_t seq_num = ParseRtpSequenceNumber(packet);
    if (!ProtectRtp(data, len, packet.capacity(), &len)) {
        RTC_LOG(LS_WARNING) << "Failed to protect rtp packet, size=" << len
            << ", seqnum=" << seq_num
            << ", ssrc=" << ParseRtpSsrc(packet)
            << ", last_send_seq_num=" << last_send_seq_num_;
        return -1;
    }
    
    last_send_seq_num_ = seq_num; 
    
    packet.SetSize(len);
    return rtp_dtls_transport_->SendPacket((const char*)packet.cdata(), packet.size());
}

int DtlsSrtpTransport::SendRtcp(const char* buf, size_t size) {
    if (!IsSrtpActive()) {
        RTC_LOG(LS_WARNING) << "Failed to send rtcp packet: Inactive srtp transport";
        return -1;
    }
   
    int rtcp_auth_tag_len = 0;
    GetSendAuthTagLen(nullptr, &rtcp_auth_tag_len);
    rtc::CopyOnWriteBuffer packet(buf, size, size + rtcp_auth_tag_len + sizeof(uint32_t)); 
    
    char* data = (char*)packet.data();
    int len = packet.size();
    if (!ProtectRtcp(data, len, packet.capacity(), &len)) {
        int type = 0;
        GetRtcpType(data, len, &type);
        RTC_LOG(LS_WARNING) << "Failed to protect rtcp packet, size=" << len
            << ", type=" << type;
        return -1;
    }
     
    packet.SetSize(len);
    return rtp_dtls_transport_->SendPacket((const char*)packet.cdata(), packet.size());
}

bool DtlsSrtpTransport::IsDtlsWritable() {
    auto rtcp_transport = rtcp_mux_enabled_ ? nullptr : rtcp_dtls_transport_;
    return rtp_dtls_transport_ && rtp_dtls_transport_->writable() &&
        (!rtcp_transport || rtcp_transport->writable());
}

void DtlsSrtpTransport::MaybeSetupDtlsSrtp() {
    if (IsSrtpActive() || !IsDtlsWritable()) {
        return;
    }

    SetupDtlsSrtp();
}

void DtlsSrtpTransport::SetupDtlsSrtp() {
    std::vector<int> send_extension_ids;
    std::vector<int> recv_extension_ids;

    int selected_crypto_suite;
    rtc::ZeroOnFreeBuffer<unsigned char> send_key;
    rtc::ZeroOnFreeBuffer<unsigned char> recv_key;

    if (!ExtractParams(rtp_dtls_transport_, &selected_crypto_suite,
                &send_key, &recv_key) ||
            !SetRtpParams(selected_crypto_suite,
                &send_key[0], send_key.size(), send_extension_ids,
                selected_crypto_suite,
                &recv_key[0], recv_key.size(), recv_extension_ids))
    {
        RTC_LOG(LS_WARNING) << "DTLS-SRTP rtp param install failed";
    }
}

bool DtlsSrtpTransport::ExtractParams(DtlsTransport* dtls_transport,
        int* selected_crypto_suite,
        rtc::ZeroOnFreeBuffer<unsigned char>* send_key,
        rtc::ZeroOnFreeBuffer<unsigned char>* recv_key)
{
    if (!dtls_transport || !dtls_transport->IsDtlsActive()) {
        return false;
    }

    if (!dtls_transport->GetSrtpCryptoSuite(selected_crypto_suite)) {
        RTC_LOG(LS_WARNING) << "No selected crypto suite";
        return false;
    }
    
    RTC_LOG(LS_INFO) << "Extract DTLS-SRTP key from transport " << transport_name_;

    int key_len;
    int salt_len;
    if (!rtc::GetSrtpKeyAndSaltLengths(*selected_crypto_suite, &key_len, &salt_len)) {
        RTC_LOG(LS_WARNING) << "Unknown DTLS-SRTP crypto suite " << *selected_crypto_suite;
        return false;
    }
    
    rtc::ZeroOnFreeBuffer<unsigned char> dtls_buffer(key_len * 2 + salt_len * 2);
    if (!dtls_transport->ExportKeyingMaterial(kDtlsSrtpExporterLabel,
                NULL, 0, false, &dtls_buffer[0], dtls_buffer.size()))
    {
        RTC_LOG(LS_WARNING) << "Extracting DTLS-SRTP param failed";
        return false;
    }
    
    rtc::ZeroOnFreeBuffer<unsigned char> client_write_key(key_len + salt_len);
    rtc::ZeroOnFreeBuffer<unsigned char> server_write_key(key_len + salt_len);
    size_t offset = 0;
    memcpy(&client_write_key[0], &dtls_buffer[offset], key_len);
    offset += key_len;
    memcpy(&server_write_key[0], &dtls_buffer[offset], key_len);
    offset += key_len;
    memcpy(&client_write_key[key_len], &dtls_buffer[offset], salt_len);
    offset += salt_len;
    memcpy(&server_write_key[key_len], &dtls_buffer[offset], salt_len);
    
    *send_key = std::move(server_write_key);
    *recv_key = std::move(client_write_key);

    return true;
}

} // namespace xrtc


