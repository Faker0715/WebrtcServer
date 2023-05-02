#include <rtc_base/logging.h>

#include "pc/srtp_transport.h"

namespace xrtc {

    SrtpTransport::SrtpTransport(bool rtcp_mux_enabled) :
            _rtcp_mux_enabled(rtcp_mux_enabled) {}

    bool SrtpTransport::is_srtp_active() {
        return _send_session && _recv_session;
    }

    bool SrtpTransport::set_rtp_params(int send_cs,
                                       const uint8_t *send_key,
                                       size_t send_key_len,
                                       const std::vector<int> &send_extension_ids,
                                       int recv_cs,
                                       const uint8_t *recv_key,
                                       size_t recv_key_len,
                                       const std::vector<int> &recv_extension_ids) {
        bool new_session = false;
        if (!_send_session) {
            _create_srtp_session();
            new_session = true;
        }

        bool ret = new_session
                   ? _send_session->set_send(send_cs, send_key, send_key_len, send_extension_ids)
                   : _send_session->update_send(send_cs, send_key, send_key_len, send_extension_ids);
        if (!ret) {
            reset_params();
            return false;
        }

        ret = new_session
              ? _recv_session->set_recv(recv_cs, recv_key, recv_key_len, recv_extension_ids)
              : _recv_session->update_recv(recv_cs, recv_key, recv_key_len, recv_extension_ids);
        if (!ret) {
            reset_params();
            return false;
        }

        RTC_LOG(LS_INFO) << "SRTP " << (new_session ? "activated" : "updated")
                         << " params: send crypto suite " << send_cs
                         << " recv crypto suite " << recv_cs;

        return true;
    }

    void SrtpTransport::reset_params() {
        _send_session = nullptr;
        _recv_session = nullptr;
        RTC_LOG(LS_INFO) << "The params in SRTP reset";
    }

    void SrtpTransport::_create_srtp_session() {
        _send_session.reset(new SrtpSession());
        _recv_session.reset(new SrtpSession());
    }

    void SrtpTransport::get_send_auth_tag_len(int *rtp_auth_tag_len, int *rtcp_auth_tag_len) {
        if (_send_session) {
            _send_session->get_auth_tag_len(rtp_auth_tag_len, rtcp_auth_tag_len);
        }
    }

    bool SrtpTransport::unprotect_rtp(void *p, int in_len, int *out_len) {
        if (!is_srtp_active()) {
            return false;
        }
        return _recv_session->unprotect_rtp(p, in_len, out_len);
    }

    bool SrtpTransport::unprotect_rtcp(void *p, int in_len, int *out_len) {
        if (!is_srtp_active()) {
            return false;
        }
        return _recv_session->unprotect_rtcp(p, in_len, out_len);
    }

    bool SrtpTransport::protect_rtp(void *p, int in_len, int max_len, int *out_len) {
        if (!is_srtp_active()) {
            return false;
        }
        return _send_session->protect_rtp(p, in_len, max_len, out_len);
    }

    bool SrtpTransport::protect_rtcp(void *p, int in_len, int max_len, int *out_len) {
        if (!is_srtp_active()) {
            return false;
        }
        return _send_session->protect_rtcp(p, in_len, max_len, out_len);
    }

} // namespace xrtc


