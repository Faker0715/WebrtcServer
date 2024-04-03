#include "pc/srtp_transport.h"

#include <rtc_base/logging.h>

namespace xrtc {

SrtpTransport::SrtpTransport(bool rtcp_mux_enabled) : 
    rtcp_mux_enabled_(rtcp_mux_enabled) {}

bool SrtpTransport::IsSrtpActive() {
    return send_session_ && recv_session_;
}

bool SrtpTransport::SetRtpParams(int send_cs,
        const uint8_t* send_key,
        size_t send_key_len,
        const std::vector<int>& send_extension_ids,
        int recv_cs,
        const uint8_t* recv_key,
        size_t recv_key_len,
        const std::vector<int>& recv_extension_ids)
{
    bool new_session = false;
    if (!send_session_) {
        CreateSrtpSession();
        new_session = true;
    }

    bool ret = new_session 
        ? send_session_->SetSend(send_cs, send_key, send_key_len, send_extension_ids)
        : send_session_->UpdateSend(send_cs, send_key, send_key_len, send_extension_ids);
    if (!ret) {
        ResetParams();
        return false;
    }

    ret = new_session 
        ? recv_session_->SetRecv(recv_cs, recv_key, recv_key_len, recv_extension_ids)
        : recv_session_->UpdateRecv(recv_cs, recv_key, recv_key_len, recv_extension_ids);
    if (!ret) {
        ResetParams();
        return false;
    }

    RTC_LOG(LS_INFO) << "SRTP " << (new_session ? "activated" : "updated")
        << " params: send crypto suite " << send_cs 
        << " recv crypto suite " << recv_cs;

    return true;
}

void SrtpTransport::ResetParams() {
    send_session_ = nullptr;
    recv_session_ = nullptr;
    RTC_LOG(LS_INFO) << "The params in SRTP reset";
}

void SrtpTransport::CreateSrtpSession() {
    send_session_.reset(new SrtpSession());
    recv_session_.reset(new SrtpSession());
}

void SrtpTransport::GetSendAuthTagLen(int* rtp_auth_tag_len, int* rtcp_auth_tag_len) {
    if (send_session_) {
        send_session_->GetAuthTagLen(rtp_auth_tag_len, rtcp_auth_tag_len);
    }
}

bool SrtpTransport::UnprotectRtp(void* p, int in_len, int* out_len) {
    if (!IsSrtpActive()) {
        return false;
    }
    return recv_session_->UnprotectRtp(p, in_len, out_len);
}   

bool SrtpTransport::UnprotectRtcp(void* p, int in_len, int* out_len) {
    if (!IsSrtpActive()) {
        return false;
    }
    return recv_session_->UnprotectRtcp(p, in_len, out_len);
} 

bool SrtpTransport::ProtectRtp(void* p, int in_len, int max_len, int* out_len) {
    if (!IsSrtpActive()) {
        return false;
    }
    return send_session_->ProtectRtp(p, in_len, max_len, out_len);
}

bool SrtpTransport::ProtectRtcp(void* p, int in_len, int max_len, int* out_len) {
    if (!IsSrtpActive()) {
        return false;
    }
    return send_session_->ProtectRtcp(p, in_len, max_len, out_len);
}

} // namespace xrtc


