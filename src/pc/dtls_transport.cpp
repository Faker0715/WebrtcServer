#include "pc/dtls_transport.h"

#include <rtc_base/logging.h>
#include <api/crypto/crypto_options.h>

namespace xrtc {

const size_t kDtlsRecordHeaderLen = 13;
const size_t kMaxDtlsPacketLen = 2048;
const size_t kMaxPendingPackets = 2;
const size_t kMinRtpPacketLen = 12;

bool IsDtlsPacket(const char* buf, size_t len) {
    const uint8_t* u = reinterpret_cast<const uint8_t*>(buf);
    return len >= kDtlsRecordHeaderLen && (u[0] > 19 && u[0] < 64);
}

bool IsDtlsClientHelloPacket(const char* buf, size_t len) {
    if (!IsDtlsPacket(buf, len)) {
        return false;
    }

    const uint8_t* u = reinterpret_cast<const uint8_t*>(buf);
    return len > 17 && (u[0] == 22 && u[13] == 1);
}

bool IsRtpPacket(const char* buf, size_t len) {
    const uint8_t* u = reinterpret_cast<const uint8_t*>(buf);
    return len >= kMinRtpPacketLen && ((u[0] & 0xC0) == 0x80);
}

StreamInterfaceChannel::StreamInterfaceChannel(IceTransportChannel* ice_channel) :
    ice_channel_(ice_channel),
    packets_(kMaxPendingPackets, kMaxDtlsPacketLen)
{
}

bool StreamInterfaceChannel::OnReceivedPacket(const char* data, size_t size) {  
    if (packets_.size() > 0) {
        RTC_LOG(LS_INFO) << ": Packet already in buffer queue";
    }
    
    if (!packets_.WriteBack(data, size, NULL)) {
        RTC_LOG(LS_WARNING) << ": Failed to write packet to queue";
    }
    
    SignalEvent(this, rtc::SE_READ, 0);

    return true;
}

rtc::StreamState StreamInterfaceChannel::GetState() const {
    return state_;
}

rtc::StreamResult StreamInterfaceChannel::Read(void* buffer,
        size_t buffer_len,
        size_t* read,
        int* /*error*/)
{
    if (state_ == rtc::SS_CLOSED) {
        return rtc::SR_EOS;
    }

    if (state_ == rtc::SS_OPENING) {
        return rtc::SR_BLOCK;
    }

    if (!packets_.ReadFront(buffer, buffer_len, read)) {
        return rtc::SR_BLOCK;
    }

    return rtc::SR_SUCCESS;
}

rtc::StreamResult StreamInterfaceChannel::Write(const void* data,
        size_t data_len,
        size_t* written,
        int* /*error*/) 
{
    ice_channel_->SendPacket((const char*)data, data_len);
    if (written) {
        *written = data_len;
    }

    return rtc::SR_SUCCESS;
}

void StreamInterfaceChannel::Close() {
    state_ = rtc::SS_CLOSED;
    packets_.Clear();
}

DtlsTransport::DtlsTransport(IceTransportChannel* ice_channel) :
    ice_channel_(ice_channel)
{
    ice_channel_->SignalReadPacket.connect(this, &DtlsTransport::OnReadPacket);
    ice_channel_->SignalWritableState.connect(this, &DtlsTransport::OnWritableState);
    ice_channel_->SignalReceivingState.connect(this, &DtlsTransport::OnReceivingState);

    webrtc::CryptoOptions crypto_options;
    srtp_ciphers_ = crypto_options.GetSupportedDtlsSrtpCryptoSuites();
}

DtlsTransport::~DtlsTransport() {

}

void DtlsTransport::OnReadPacket(IceTransportChannel* /*channel*/,
        const char* buf, size_t len, int64_t ts)
{
    switch (dtls_state_) {
        case DtlsTransportState::kNew:
            if (dtls_) {
                RTC_LOG(LS_INFO) << ToString() << ": Received packet before DTLS started.";
            } else {
                RTC_LOG(LS_WARNING) << ToString() << ": Received packet before we know if "
                    << "we are doing DTLS or not";
            }
            
            if (IsDtlsClientHelloPacket(buf, len)) {
                RTC_LOG(LS_INFO) << ToString() << ": Caching DTLS ClientHello packet until "
                    << "DTLS started";
                cached_client_hello_.SetData(buf, len);

                if (!dtls_ && local_certificate_) {
                    SetupDtls();
                }

            } else {
                RTC_LOG(LS_WARNING) << ToString() << ": Not a DTLS ClientHello packet, "
                    << "dropping";
            }

            break;
        case DtlsTransportState::kConnecting:
        case DtlsTransportState::kConnected:
            if (IsDtlsPacket(buf, len)) { // Dtls包
                if (!HandleDtlsPacket(buf, len)) {
                    RTC_LOG(LS_WARNING) << ToString() << ": handle DTLS packet failed";
                    return;
                }
            } else { // RTP/RTCP包
                if (dtls_state_ != DtlsTransportState::kConnected) {
                    RTC_LOG(LS_WARNING) << ToString() << ": Received non DTLS packet "
                        << "before DTLS complete";
                    return;
                }

                if (!IsRtpPacket(buf, len)) {
                    RTC_LOG(LS_WARNING) << ToString() << ": Received unexpected non "
                        << "DTLS packet";
                    return;
                }

                SignalReadPacket(this, buf, len, ts);
            }

            break;
        default:
            break;
    }
}

void DtlsTransport::OnWritableState(IceTransportChannel* channel) {
    RTC_LOG(LS_INFO) << ToString() << ": IceTransportChannel writable changed to "
        << channel->writable();

    if (!dtls_active_) {
        set_writable_state(channel->writable());
        return;
    }

    switch (dtls_state_) {
        case DtlsTransportState::kNew:
            MaybeStartDtls();
            break;
        case DtlsTransportState::kConnected:
            set_writable_state(channel->writable());
            break;
        default:
            break;
    }
}

void DtlsTransport::OnReceivingState(IceTransportChannel* channel) {
    set_receiving(channel->receiving());
}

void DtlsTransport::set_receiving(bool receiving) {
    if (receiving_ == receiving) {
        return;
    }

    RTC_LOG(LS_INFO) << ToString() << ": Change receiving to " << receiving;
    receiving_ = receiving;
    SignalReceivingState(this);
}

bool DtlsTransport::SetLocalCertificate(rtc::RTCCertificate* cert) {
    if (dtls_active_) {
        if (cert == local_certificate_) {
            RTC_LOG(LS_INFO) << ToString() << ": Ingnoring identical DTLS cert";
            return true;
        } else {
            RTC_LOG(LS_WARNING) << ToString() << ": Cannot change cert in this state";
            return false;
        }
    }

    if (cert) {
        local_certificate_ = cert;
        dtls_active_ = true;
    }

    return true;
}

bool DtlsTransport::SetRemoteFingerprint(const std::string& digest_alg,
        const uint8_t* digest, size_t digest_len)
{
    rtc::Buffer remote_fingerprint_value(digest, digest_len);

    if (dtls_active_ && remote_fingerprint_value_ == remote_fingerprint_value &&
            !digest_alg.empty())
    {
        RTC_LOG(LS_INFO) << ToString() << ": Ignoring identical remote fingerprint";
        return true;
    }

    if (digest_alg.empty()) {
        RTC_LOG(LS_WARNING) << ToString() << ": Other sides not support DTLS";
        dtls_active_ = false;
        return false;
    }

    if (!dtls_active_) {
        RTC_LOG(LS_WARNING) << ToString() << ": Cannot set remote fingerpint in this state";
        return false;
    }

    bool fingerprint_change = remote_fingerprint_value_.size() > 0u;
    remote_fingerprint_value_ = std::move(remote_fingerprint_value);
    remote_fingerprint_alg_ = digest_alg;

    // ClientHello packet先到，answer sdp后到
    if (dtls_ && !fingerprint_change) {
        rtc::SSLPeerCertificateDigestError err;
        if (!dtls_->SetPeerCertificateDigest(digest_alg, (const unsigned char*)digest, 
                    digest_len, &err)) 
        {
            RTC_LOG(LS_WARNING) << ToString() << ": Failed to set peer certificate digest";
            set_dtls_state(DtlsTransportState::kFailed);
            return err == rtc::SSLPeerCertificateDigestError::VERIFICATION_FAILED;
        }

        return true;
    }
    
    if (dtls_ && fingerprint_change) {
        dtls_.reset(nullptr);
        set_dtls_state(DtlsTransportState::kNew);
        set_writable_state(false);
    }
    
    if (!SetupDtls()) {
        RTC_LOG(LS_WARNING) << ToString() << ": Failed to setup DTLS";
        set_dtls_state(DtlsTransportState::kFailed);
        return false;
    }
    
    return true;
}

void DtlsTransport::set_dtls_state(DtlsTransportState state) {
    if (dtls_state_ == state) {
        return;
    }

    RTC_LOG(LS_INFO) << ToString() << ": Change dtls state from " << dtls_state_
        << " to " << state;
    dtls_state_ = state;
    SignalDtlsState(this, state);
}

void DtlsTransport::set_writable_state(bool writable) {
    if (writable_ == writable) {
        return;
    }

    RTC_LOG(LS_INFO) << ToString() << ": set DTLS writable to " << writable;
    writable_ = writable;
    SignalWritableState(this);
}

bool DtlsTransport::SetupDtls() {
    auto downward = std::make_unique<StreamInterfaceChannel>(ice_channel_); 
    StreamInterfaceChannel* downward_ptr = downward.get();

    dtls_ = rtc::SSLStreamAdapter::Create(std::move(downward));
    if (!dtls_) {
        RTC_LOG(LS_WARNING) << ToString() << ": Failed to create SSLStreamAdapter";
        return false;
    }

    downward_ = downward_ptr;
    
    dtls_->SetIdentity(local_certificate_->identity()->Clone());
    dtls_->SetMode(rtc::SSL_MODE_DTLS);
    dtls_->SetMaxProtocolVersion(rtc::SSL_PROTOCOL_DTLS_12);
    dtls_->SetServerRole(rtc::SSL_SERVER);
    dtls_->SignalEvent.connect(this, &DtlsTransport::OnDtlsEvent);
    dtls_->SignalSSLHandshakeError.connect(this, 
            &DtlsTransport::OnDtlsHandshakeError);

    if (remote_fingerprint_value_.size() && !dtls_->SetPeerCertificateDigest(
                remote_fingerprint_alg_,
                remote_fingerprint_value_.data(),
                remote_fingerprint_value_.size()))
    {
        RTC_LOG(LS_WARNING) << ToString() << ": Failed to set remote fingerprint";
        return false;
    }
    
    if (!srtp_ciphers_.empty()) {
        if (!dtls_->SetDtlsSrtpCryptoSuites(srtp_ciphers_)) {
            RTC_LOG(LS_WARNING) << ToString() << ": Failed to set DTLS-SRTP crypto suites";
            return false;
        }
    } else {
        RTC_LOG(LS_WARNING) << ToString() << ": Not using DTLS-SRTP";
    }

    RTC_LOG(LS_INFO) << ToString() << ": Setup DTLS complete";
    
    MaybeStartDtls();

    return true;
}

void DtlsTransport::OnDtlsEvent(rtc::StreamInterface* /*dtls*/, int sig, int error) {
    if (sig & rtc::SE_OPEN) {
        RTC_LOG(LS_INFO) << ToString() << ": DTLS handshake complete.";
        set_writable_state(true);
        set_dtls_state(DtlsTransportState::kConnected);
    }

    if (sig & rtc::SE_READ) {
        char buf[kMaxDtlsPacketLen];
        size_t read;
        int read_error;
        rtc::StreamResult ret;
        // 因为一个数据包可能会包含多个DTLS record，需要循环读取
        do {
            ret = dtls_->Read(buf, sizeof(buf), &read, &read_error);
            if (ret == rtc::SR_SUCCESS) {
            } else if (ret == rtc::SR_EOS) {
                RTC_LOG(LS_INFO) << ToString() << ": DTLS transport closed by remote.";
                set_writable_state(false);
                set_dtls_state(DtlsTransportState::kClosed);
                SignalClosed(this);
            } else if (ret == rtc::SR_ERROR) {
                RTC_LOG(LS_WARNING) << ToString() << ": Closed DTLS transport by remote "
                    << "with error, code=" << read_error;
                set_writable_state(false);
                set_dtls_state(DtlsTransportState::kFailed);
                SignalClosed(this);
            }

        } while (ret == rtc::SR_SUCCESS);
    }

    if (sig & rtc::SE_CLOSE) {
        if (!error) {
            RTC_LOG(LS_INFO) << ToString() << ": DTLS transport closed";
            set_writable_state(false);
            set_dtls_state(DtlsTransportState::kClosed);
        } else {
            RTC_LOG(LS_INFO) << ToString() << ": DTLS transport closed with error, "
                << "code=" << error;
            set_writable_state(false);
            set_dtls_state(DtlsTransportState::kFailed);
        }
    }
}

void DtlsTransport::OnDtlsHandshakeError(rtc::SSLHandshakeError err) {
    RTC_LOG(LS_WARNING) << ToString() << ": DTLS handshake error=" << (int)err;
}

void DtlsTransport::MaybeStartDtls() {
    if (dtls_ && ice_channel_->writable()) {
        if (dtls_->StartSSL()) {
            RTC_LOG(LS_WARNING) << ToString() << ": Failed to StartSSL.";
            set_dtls_state(DtlsTransportState::kFailed);
            return;
        }

        RTC_LOG(LS_INFO) << ToString() << ": Started DTLS.";
        set_dtls_state(DtlsTransportState::kConnecting);

        if (cached_client_hello_.size()) {
            if (!HandleDtlsPacket(cached_client_hello_.data<char>(),
                        cached_client_hello_.size()))
            {
                RTC_LOG(LS_WARNING) << ToString() << ": Handling dtls packet failed.";
                set_dtls_state(DtlsTransportState::kFailed);
            }
            cached_client_hello_.Clear();
        }
    }
}

bool DtlsTransport::HandleDtlsPacket(const char* data, size_t size) {
    const uint8_t* tmp_data = reinterpret_cast<const uint8_t*>(data);
    size_t tmp_size = size;

    while (tmp_size > 0) {
        if (tmp_size < kDtlsRecordHeaderLen) {
            return false;
        }

        size_t record_len = (tmp_data[11] << 8) | tmp_data[12];
        if (record_len + kDtlsRecordHeaderLen > tmp_size) {
            return false;
        }

        tmp_data += kDtlsRecordHeaderLen + record_len;
        tmp_size -= kDtlsRecordHeaderLen + record_len;
    }

    return downward_->OnReceivedPacket(data, size);
}

std::string DtlsTransport::ToString() {
    std::stringstream ss;
    absl::string_view RECEIVING[2] = {"-", "R"};
    absl::string_view WRITABLE[2] = {"-", "W"};

    ss << "DtlsTransport[" << transport_name() << "|"
        << (int)component() << "|"
        << RECEIVING[receiving_] << "|"
        << WRITABLE[writable_] << "]";
    return ss.str();
}

bool DtlsTransport::GetSrtpCryptoSuite(int* selected_crypto_suite) {
    if (dtls_state_ != DtlsTransportState::kConnected) {
        return false;
    }

    return dtls_->GetDtlsSrtpCryptoSuite(selected_crypto_suite);
}

bool DtlsTransport::ExportKeyingMaterial(const std::string& label,
        const uint8_t* context,
        size_t context_len,
        bool use_context,
        uint8_t* result,
        size_t result_len)
{
    return dtls_.get() ? dtls_->ExportKeyingMaterial(label, context, context_len,
            use_context, result, result_len) : false;
}

int DtlsTransport::SendPacket(const char* data, size_t len) {
    if (ice_channel_) {
        return ice_channel_->SendPacket(data, len);
    }

    return -1;
}

} // namespace xrtc

















