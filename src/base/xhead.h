#ifndef  __BASE_XHEAD_H_
#define  __BASE_XHEAD_H_

#include <stdint.h>

namespace xrtc {

const int XHEAD_SIZE = 36;
const uint32_t XHEAD_MAGIC_NUM = 0xfb202202;

struct xhead_t {
    uint16_t id;
    uint16_t version;
    uint32_t log_id;
    char provider[16];
    uint32_t magic_num;
    uint32_t reserved;
    uint32_t body_len;
};

} // namespace xrtc

#endif  //__BASE_XHEAD_H_


