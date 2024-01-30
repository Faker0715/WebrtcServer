

#ifndef  __PC_STREAM_PARAMS_H_
#define  __PC_STREAM_PARAMS_H_

#include <vector>
#include <string>
#include <stdint.h>
namespace xrtc {

struct SsrcGroup {
    SsrcGroup(const std::string& semantics, const std::vector<uint32_t>& ssrcs);

    std::string semantics;
    std::vector<uint32_t> ssrcs;
};

struct StreamParams {
    bool has_ssrc(uint32_t ssrc);

    uint32_t FirstSsrc() const{
        if(ssrcs.empty()){
            return 0;
        }
        return ssrcs[0];
    }
    std::string id;
    std::vector<uint32_t> ssrcs;
    std::vector<SsrcGroup> ssrc_groups;
    std::string cname;
    std::string stream_id;
};

} // namespace xrtc


#endif  //__PC_STREAM_PARAMS_H_


