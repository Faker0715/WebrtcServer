

#ifndef  __SRTP_SESSION_H_
#define  __SRTP_SESSION_H_

#include <vector>
#include <string>

#include <srtp2/srtp.h>

namespace xrtc {

class SrtpSession {
public:
    SrtpSession();
    ~SrtpSession();
    
    bool set_send(int cs, const uint8_t* key, size_t key_len,
            const std::vector<int>& extension_ids);
    bool update_send(int cs, const uint8_t* key, size_t key_len,
            const std::vector<int>& extension_ids);
    bool set_recv(int cs, const uint8_t* key, size_t key_len,
            const std::vector<int>& extension_ids);
    bool update_recv(int cs, const uint8_t* key, size_t key_len,
            const std::vector<int>& extension_ids);
    bool unprotect_rtp(void* p, int in_len, int* out_len);
    bool unprotect_rtcp(void* p, int in_len, int* out_len);
    bool protect_rtp(void* p, int in_len, int max_len, int* out_len);
    void get_auth_tag_len(int* rtp_auth_tag_len, int* rtcp_auth_tag_len);

private:
    bool _set_key(int type, int cs, const uint8_t* key, size_t key_len,
        const std::vector<int>& extension_ids);
    bool _update_key(int type, int cs, const uint8_t* key, size_t key_len,
            const std::vector<int>& extension_ids);
    bool _do_set_key(int type, int cs, const uint8_t* key, size_t key_len,
            const std::vector<int>& extension_ids);
    static bool _increment_libsrtp_usage_count_and_maybe_init();
    static void _event_handle_thunk(srtp_event_data_t* ev);
    void _handle_event(srtp_event_data_t* ev);
   
private:
    srtp_ctx_t_* _session = nullptr;
    bool _inited = false;
    int _rtp_auth_tag_len = 0;
    int _rtcp_auth_tag_len = 0;
};

} // namespace xrtc

#endif  //__SRTP_SESSION_H_


