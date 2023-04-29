//
// Created by faker on 23-4-29.
//

#ifndef XRTCSERVER_SRTP_SESSION_H
#define XRTCSERVER_SRTP_SESSION_H
#include <srtp2/srtp.h>
#include <vector>
#include <cstddef>

namespace xrtc{
class SrtpSession{
public:
    SrtpSession();
    ~SrtpSession();
    bool set_send(int cs, const uint8_t* key, size_t key_len, const std::vector<int>& extension_ids);
private:
    bool _set_key(int type, int cs, const uint8_t *key, size_t key_len, const std::vector<int> &extension_ids);
    static bool _increment_libsrtp_usage_count_and_maybe_init();
    void _handle_event(srtp_event_data_t *ev);
    bool _do_set_key(int type, int cs, const uint8_t *key, size_t key_len, const std::vector<int>);
    static void _event_handle_thunk(srtp_event_data_t *ev);
private:
    srtp_ctx_t_* _session = nullptr;
    bool _inited = false;
    int _rtp_auth_tag_len = 0;
    int _rtcp_auth_tag_len = 0;

};
}


#endif //XRTCSERVER_SRTP_SESSION_H
