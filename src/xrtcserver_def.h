#ifndef  __XRTCSERVER_DEF_H_
#define  __XRTCSERVER_DEF_H_

#define CMDNO_PUSH     1
#define CMDNO_PULL     2
#define CMDNO_ANSWER   3
#define CMDNO_STOPPUSH 4
#define CMDNO_STOPPULL 5

#define MAX_RES_BUF 4096

namespace xrtc {

struct RtcMsg {
    int cmdno = -1;
    uint64_t uid = 0;
    std::string stream_name;
    std::string stream_type;
    int audio = 0;
    int video = 0;
    int is_dtls = 1;
    uint32_t log_id = 0;
    void* worker = nullptr;
    void* conn = nullptr;
    int fd = 0;
    std::string sdp;
    int err_no = 0;
    void* certificate = nullptr;
};

} // namespace xrtc


#endif  //__XRTCSERVER_DEF_H_


