#include "pc/srtp_session.h"

#include <rtc_base/logging.h>
#include <rtc_base/synchronization/mutex.h>
#include <absl/base/attributes.h>

namespace xrtc {

SrtpSession::SrtpSession() {
}

SrtpSession::~SrtpSession() {
    if (session_) {
        srtp_set_user_data(session_, nullptr);
        srtp_dealloc(session_);
    }

    if (inited_) {
        DecrementLibsrtpUsageCountAndMaybeDeinit();
    }
}

bool SrtpSession::SetSend(int cs, const uint8_t* key, size_t key_len,
        const std::vector<int>& extension_ids)
{
    return SetKey(ssrc_any_outbound, cs, key, key_len, extension_ids);
}

bool SrtpSession::UpdateSend(int cs, const uint8_t* key, size_t key_len,
        const std::vector<int>& extension_ids)
{
    return UpdateKey(ssrc_any_outbound, cs, key, key_len, extension_ids);
}

bool SrtpSession::SetRecv(int cs, const uint8_t* key, size_t key_len,
        const std::vector<int>& extension_ids)
{
    return SetKey(ssrc_any_inbound, cs, key, key_len, extension_ids);
}

bool SrtpSession::UpdateRecv(int cs, const uint8_t* key, size_t key_len,
        const std::vector<int>& extension_ids)
{
    return UpdateKey(ssrc_any_inbound, cs, key, key_len, extension_ids);
}

bool SrtpSession::UpdateKey(int type, int cs, const uint8_t* key, size_t key_len,
        const std::vector<int>& extension_ids)
{
    if (!session_) {
        RTC_LOG(LS_WARNING) << "Failed to update on non-existing SRTP session";
        return false;
    }

    return DoSetKey(type, cs, key, key_len, extension_ids);
}

ABSL_CONST_INIT int g_libsrtp_usage_count = 0;
ABSL_CONST_INIT webrtc::GlobalMutex g_libsrtp_lock(absl::kConstInit);

void SrtpSession::EventHandleThunk(srtp_event_data_t* ev) {
    SrtpSession* session = (SrtpSession*)(srtp_get_user_data(ev->session));
    if (session) {
        session->HandleEvent(ev);
    }
}

void SrtpSession::HandleEvent(srtp_event_data_t* ev) {
    switch (ev->event) {
        case event_ssrc_collision:
            RTC_LOG(LS_INFO) << "SRTP event: ssrc collision";
            break;
        case event_key_soft_limit:
            RTC_LOG(LS_INFO) << "SRTP event: reached key soft limit";
            break;
        case event_key_hard_limit:
            RTC_LOG(LS_INFO) << "SRTP event: reached key hard limit";
            break;
        case event_packet_index_limit:
            RTC_LOG(LS_INFO) << "SRTP event: packet index limit";
            break;
        default:
            RTC_LOG(LS_WARNING) << "SRTP unknown event: " << ev->event;
            break;
    }
}

bool SrtpSession::IncrementLibsrtpUsageCountAndMaybeInit() {
    webrtc::GlobalMutexLock ls(&g_libsrtp_lock);
    
    if (0 == g_libsrtp_usage_count) {
        int err = srtp_init();
        if (err != srtp_err_status_ok) {
            RTC_LOG(LS_WARNING) << "Failed to init srtp, err: " << err;
            return false;
        }

        err = srtp_install_event_handler(&SrtpSession::EventHandleThunk);
        if (err != srtp_err_status_ok) {
            RTC_LOG(LS_WARNING) << "Failed to install srtp event, err: " << err;
            return false;
        }
    }
    
    g_libsrtp_usage_count++;
    return true;
}

void SrtpSession::DecrementLibsrtpUsageCountAndMaybeDeinit() {
    webrtc::GlobalMutexLock ls(&g_libsrtp_lock);
    
    if (--g_libsrtp_usage_count == 0) {
        int err = srtp_shutdown();
        if (err) {
            RTC_LOG(LS_WARNING) << "Failed to shutdown srtp, err: " << err;
        }    
    }    
}

bool SrtpSession::SetKey(int type, int cs, const uint8_t* key, size_t key_len,
        const std::vector<int>& extension_ids)
{
    if (session_) {
        RTC_LOG(LS_WARNING) << "Failed to create session: "
            << "SRTP session already created";
        return false;
    }

    if (IncrementLibsrtpUsageCountAndMaybeInit()) {
        inited_ = true;
    } else {
        return false;
    }

    return DoSetKey(type, cs, key, key_len, extension_ids);
}

bool SrtpSession::DoSetKey(int type, int cs, const uint8_t* key, size_t key_len,
        const std::vector<int>& /*extension_ids*/)
{
    srtp_policy_t policy;
    memset(&policy, 0, sizeof(policy));

    bool rtp_ret = srtp_crypto_policy_set_from_profile_for_rtp(
            &policy.rtp, (srtp_profile_t)cs);
    bool rtcp_ret = srtp_crypto_policy_set_from_profile_for_rtcp(
            &policy.rtcp, (srtp_profile_t)cs);
    if (rtp_ret != srtp_err_status_ok || rtcp_ret != srtp_err_status_ok) {
        RTC_LOG(LS_WARNING) << "SRTP session " << (session_ ? "create" : "update")
            << " failed: unsupported crypto suite " << cs;
        return false;
    }

    if (!key || key_len != (size_t)policy.rtp.cipher_key_len) {
        RTC_LOG(LS_WARNING) << "SRTP session " << (session_ ? "create" : "update")
            << " failed: invalid key";
        return false;
    }

    policy.ssrc.type = (srtp_ssrc_type_t)type;
    policy.ssrc.value = 0;
    policy.key = (uint8_t*)key;
    policy.window_size = 1024;
    policy.allow_repeat_tx = 1;
    policy.next = nullptr;

    if (!session_) {
        int err = srtp_create(&session_, &policy);
        if (err != srtp_err_status_ok) {
            RTC_LOG(LS_WARNING) << "Failed to create srtp, err: " << err;
            session_ = nullptr;
            return false;
        }
        srtp_set_user_data(session_, this);
    } else {
        int err = srtp_update(session_, &policy);
        if (err != srtp_err_status_ok) {
            RTC_LOG(LS_WARNING) << "Failed to update srtp, err: " << err;
            return false;
        }
    }

    rtp_auth_tag_len_ = policy.rtp.auth_tag_len;
    rtcp_auth_tag_len_ = policy.rtcp.auth_tag_len;
    return true;
}

void SrtpSession::GetAuthTagLen(int* rtp_auth_tag_len, int* rtcp_auth_tag_len) {
    if (!session_) {
        RTC_LOG(LS_WARNING) << "Failed to get auth tag len: no SRTP session";
        return;
    }

    if (rtp_auth_tag_len) {
        *rtp_auth_tag_len = rtp_auth_tag_len_;
    }

    if (rtcp_auth_tag_len) {
        *rtcp_auth_tag_len = rtcp_auth_tag_len_;
    }
}

bool SrtpSession::UnprotectRtp(void* p, int in_len, int* out_len) {
    if (!session_) {
        RTC_LOG(LS_WARNING) << "Failed to unprotect rtp packet: no SRTP session";
        return false;
    }
    
    *out_len = in_len;
    int err = srtp_unprotect(session_, p, out_len);
    return err == srtp_err_status_ok;
}

bool SrtpSession::UnprotectRtcp(void* p, int in_len, int* out_len) {
    if (!session_) {
        RTC_LOG(LS_WARNING) << "Failed to unprotect rtcp packet: no SRTP session";
        return false;
    }
    
    *out_len = in_len;
    int err = srtp_unprotect_rtcp(session_, p, out_len);
    return err == srtp_err_status_ok;
}

bool SrtpSession::ProtectRtp(void* p, int in_len, int max_len, int* out_len) {
    if (!session_) {
        RTC_LOG(LS_WARNING) << "Failed to protect rtp packet: no SRTP session";
        return false;
    }

    int need_len = in_len + rtp_auth_tag_len_;
    if (max_len < need_len) {
        RTC_LOG(LS_WARNING) << "Failed to protect rtp packet: The buffer length "
            << max_len << " is less than needed " << need_len;
        return false;
    }

    *out_len = in_len;
    int err = srtp_protect(session_, p, out_len);
    if (err != srtp_err_status_ok) {
        RTC_LOG(LS_WARNING) << "Failed to protect rtp packet: err=" << err;
        return false;
    }

    return true;
}

bool SrtpSession::ProtectRtcp(void* p, int in_len, int max_len, int* out_len) {
    if (!session_) {
        RTC_LOG(LS_WARNING) << "Failed to protect rtcp packet: no SRTP session";
        return false;
    }

    int need_len = in_len + rtcp_auth_tag_len_ + sizeof(uint32_t);
    if (max_len < need_len) {
        RTC_LOG(LS_WARNING) << "Failed to protect rtcp packet: The buffer length "
            << max_len << " is less than needed " << need_len;
        return false;
    }

    *out_len = in_len;
    int err = srtp_protect_rtcp(session_, p, out_len);
    if (err != srtp_err_status_ok) {
        RTC_LOG(LS_WARNING) << "Failed to protect rtcp packet: err=" << err;
        return false;
    }

    return true;
}

} // namespace xrtc


