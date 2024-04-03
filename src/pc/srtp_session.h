#ifndef  __XRTCSERVER_PC_SRTP_SESSION_H_
#define  __XRTCSERVER_PC_SRTP_SESSION_H_

#include <vector>
#include <string>

#include <srtp2/srtp.h>

namespace xrtc {

class SrtpSession {
public:
    SrtpSession();
    ~SrtpSession();
    
    bool SetSend(int cs, const uint8_t* key, size_t key_len,
            const std::vector<int>& extension_ids);
    bool UpdateSend(int cs, const uint8_t* key, size_t key_len,
            const std::vector<int>& extension_ids);
    bool SetRecv(int cs, const uint8_t* key, size_t key_len,
            const std::vector<int>& extension_ids);
    bool UpdateRecv(int cs, const uint8_t* key, size_t key_len,
            const std::vector<int>& extension_ids);
    bool UnprotectRtp(void* p, int in_len, int* out_len);
    bool UnprotectRtcp(void* p, int in_len, int* out_len);
    bool ProtectRtp(void* p, int in_len, int max_len, int* out_len);
    bool ProtectRtcp(void* p, int in_len, int max_len, int* out_len);
    void GetAuthTagLen(int* rtp_auth_tag_len, int* rtcp_auth_tag_len);

private:
    bool SetKey(int type, int cs, const uint8_t* key, size_t key_len,
        const std::vector<int>& extension_ids);
    bool UpdateKey(int type, int cs, const uint8_t* key, size_t key_len,
            const std::vector<int>& extension_ids);
    bool DoSetKey(int type, int cs, const uint8_t* key, size_t key_len,
            const std::vector<int>& extension_ids);
    static bool IncrementLibsrtpUsageCountAndMaybeInit();
    static void DecrementLibsrtpUsageCountAndMaybeDeinit();
    static void EventHandleThunk(srtp_event_data_t* ev);
    void HandleEvent(srtp_event_data_t* ev);
   
private:
    srtp_ctx_t_* session_ = nullptr;
    bool inited_ = false;
    int rtp_auth_tag_len_ = 0;
    int rtcp_auth_tag_len_ = 0;
};

} // namespace xrtc

#endif  //__XRTCSERVER_PC_SRTP_SESSION_H_


