//
// Created by faker on 23-4-29.
//

#ifndef XRTCSERVER_STREAM_PARAMS_H
#define XRTCSERVER_STREAM_PARAMS_H

#include <string>
#include <vector>

namespace xrtc{

    struct SsrcGroup {
        SsrcGroup(const std::string& semantics, const std::vector<uint32_t>& ssrcs);

        std::string semantics;
        std::vector<uint32_t> ssrcs;
    };
}


#endif //XRTCSERVER_STREAM_PARAMS_H
