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

    bool SrtpSession::set_send(int cs, const uint8_t *key, size_t key_len, const std::vector<int> &extension_ids) {
        return _set_key(ssrc_any_outbound,cs,key,key_len,extension_ids);
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
        return true;

    }
}