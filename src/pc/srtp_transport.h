#ifndef  __XRTCSERVER_PC_SRTP_TRANSPORT_H_
#define  __XRTCSERVER_PC_SRTP_TRANSPORT_H_

#include <memory>
#include <string>
#include <vector>

#include <rtc_base/third_party/sigslot/sigslot.h>

#include "pc/srtp_session.h"

namespace xrtc {

class SrtpTransport : public sigslot::has_slots<> {
public:
    SrtpTransport(bool rtcp_mux_enabled);
    virtual ~SrtpTransport() = default;
    
    bool SetRtpParams(int send_cs,
            const uint8_t* send_key,
            size_t send_key_len,
            const std::vector<int>& send_extension_ids,
            int recv_cs,
            const uint8_t* recv_key,
            size_t recv_key_len,
            const std::vector<int>& recv_extension_ids);
    void ResetParams();
    bool IsSrtpActive();
    bool UnprotectRtp(void* p, int in_len, int* out_len);
    bool UnprotectRtcp(void* p, int in_len, int* out_len);
    bool ProtectRtp(void* p, int in_len, int max_len, int* out_len);
    bool ProtectRtcp(void* p, int in_len, int max_len, int* out_len);
    void GetSendAuthTagLen(int* rtp_auth_tag_len, int* rtcp_auth_tag_len);

private:
    void CreateSrtpSession();

protected:
    bool rtcp_mux_enabled_;
    std::unique_ptr<SrtpSession> send_session_;
    std::unique_ptr<SrtpSession> recv_session_;
};

} // namespace xrtc

#endif  //__XRTCSERVER_PC_SRTP_TRANSPORT_H_


