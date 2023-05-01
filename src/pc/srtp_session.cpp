//
// Created by faker on 23-4-29.
//

#include "srtp_session.h"
#include "rtc_base/logging.h"
#include "rtc_base/synchronization/mutex.h"

namespace xrtc{

    SrtpSession::SrtpSession() {

    }

    SrtpSession::~SrtpSession() {

    }

    bool SrtpSession::set_send(int cs, const uint8_t* key, size_t key_len,
                               const std::vector<int>& extension_ids)
    {
        return _set_key(ssrc_any_outbound, cs, key, key_len, extension_ids);
    }

    bool SrtpSession::update_send(int cs, const uint8_t* key, size_t key_len,
                                  const std::vector<int>& extension_ids)
    {
        return _update_key(ssrc_any_outbound, cs, key, key_len, extension_ids);
    }

    bool SrtpSession::set_recv(int cs, const uint8_t* key, size_t key_len,
                               const std::vector<int>& extension_ids)
    {
        return _set_key(ssrc_any_outbound, cs, key, key_len, extension_ids);
    }

    bool SrtpSession::update_recv(int cs, const uint8_t* key, size_t key_len,
                                  const std::vector<int>& extension_ids)
    {
        return _update_key(ssrc_any_outbound, cs, key, key_len, extension_ids);
    }

    bool SrtpSession::_update_key(int type, int cs, const uint8_t* key, size_t key_len,
                                  const std::vector<int>& extension_ids)
    {
        if (!_session) {
            RTC_LOG(LS_WARNING) << "Failed to update on non-exsiting SRTP session";
            return false;
        }

        return _do_set_key(type, cs, key, key_len, extension_ids);
    }

    ABSL_CONST_INIT int g_libsrtp_usage_count = 0;
    ABSL_CONST_INIT webrtc::GlobalMutex g_libsrtp_lock(absl::kConstInit);
    void SrtpSession::_event_handle_thunk(srtp_event_data_t* ev){
        SrtpSession* session = (SrtpSession*)(srtp_get_user_data(ev->session));
        if(session){
            session->_handle_event(ev);
        }
    }
    void SrtpSession::_handle_event(srtp_event_data_t* ev){
        switch(ev->event){
            case event_ssrc_collision:
                RTC_LOG(LS_INFO) << "SRTP event: ssrc collision";
                break;
            case event_key_soft_limit:
                RTC_LOG(LS_INFO) << "SRTP event: key soft limit reached";
                break;
            case event_key_hard_limit:
                RTC_LOG(LS_INFO) << "SRTP event: key hard limit reached";
                break;
            case event_packet_index_limit:
                RTC_LOG(LS_INFO) << "SRTP event: packet index limit reached";
                break;
            default:
                RTC_LOG(LS_WARNING) << "SRTP event: unknown event: " << ev->event;
                break;
        }
    }
    bool SrtpSession::_increment_libsrtp_usage_count_and_maybe_init(){
        webrtc::GlobalMutexLock lock(&g_libsrtp_lock);
        if(0 == g_libsrtp_usage_count){
            int err = srtp_init();
            if(err != srtp_err_status_ok){
                RTC_LOG(LS_WARNING) << "Failed to init srtp, err: " << err;
                return false;
            }
            // 初始化成功 设置事件处理函数
            err = srtp_install_event_handler(&SrtpSession::_event_handle_thunk);
            if(err != srtp_err_status_ok){
                RTC_LOG(LS_WARNING) << "Failed to install srtp event, err: " << err;
                return false;
            }
        }
        g_libsrtp_usage_count++;

        return true;

    }
    bool SrtpSession::_set_key(int type,int cs,const uint8_t *key, size_t key_len, const std::vector<int> &extension_ids) {
        // libsrtp只会全局初始化一次
        if(_session){
            RTC_LOG(LS_WARNING) << "Failed to create session: "
                << "SRTP session already created";
            return false;
        }
        if(_increment_libsrtp_usage_count_and_maybe_init()){
            _inited = true;
        }else{
            return false;
        }
        return _do_set_key(type,cs,key,key_len,extension_ids);
    }
    bool SrtpSession::_do_set_key(int type,int cs,const uint8_t *key, size_t key_len,
                                  const std::vector<int> /*&extension_ids*/) {
        srtp_policy_t policy;
        memset(&policy,0,sizeof(policy));
        bool rtp_ret = srtp_crypto_policy_set_from_profile_for_rtp(&policy.rtp,(srtp_profile_t)cs);
        bool rtcp_ret = srtp_crypto_policy_set_from_profile_for_rtcp(&policy.rtp,(srtp_profile_t)cs);
        if(rtp_ret != srtp_err_status_ok || rtcp_ret != srtp_err_status_ok){
            RTC_LOG(LS_WARNING) << "SRTP session " << (_session ? "create" : "update")
                << " failed: unsupported crypto suite" << cs;
            return false;
        }

        if(!key || key_len != (size_t)policy.rtp.cipher_key_len){
            RTC_LOG(LS_WARNING) << "SRTP session " << (_session ? "create" : "update")
                << " failed: invalid key";
            return false;
        }
        policy.ssrc.type = (srtp_ssrc_type_t)type;
        policy.ssrc.value = 0;
        policy.key = (uint8_t* )key;
        policy.window_size = 1024;
        policy.allow_repeat_tx = 1;
        if(!_session){
            int err = srtp_create(&_session,&policy);
            if(err != srtp_err_status_ok){
                RTC_LOG(LS_WARNING) << "Failed to create srtp, err: " << err;
                _session = nullptr;
                return false;
            }
            srtp_set_user_data(_session,this);
        }else{
            int err = srtp_update(_session,&policy);
            if(err != srtp_err_status_ok){
                RTC_LOG(LS_WARNING) << "Failed to update srtp, err: " << err;
                return false;
            }
        }
        _rtp_auth_tag_len = policy.rtp.auth_tag_len;
        _rtcp_auth_tag_len = policy.rtcp.auth_tag_len;
        return true;
    }

    bool SrtpSession::unprotect_rtp(void *p, int in_len, int *out_len) {
        if(!_session){
            RTC_LOG(LS_WARNING) << "Failed to unprotect rtp on non-existing SRTP session";
            return false;
        }
        *out_len = in_len;
        int err = srtp_unprotect(_session,p,out_len);
        return err  == srtp_err_status_ok;

    }

    bool SrtpSession::unprotect_rtcp(void *p, int in_len, int *out_len) {

        if(!_session){
            RTC_LOG(LS_WARNING) << "Failed to unprotect rtcp on non-existing SRTP session";
            return false;
        }
        *out_len = in_len;
        int err = srtp_unprotect_rtcp(_session,p,out_len);
        return err  == srtp_err_status_ok;
    }
}