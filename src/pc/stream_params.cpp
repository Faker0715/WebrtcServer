//
// Created by faker on 23-4-29.
//

#include "stream_params.h"
namespace xrtc{

    SsrcGroup::SsrcGroup(const std::string &semantics, const std::vector<uint32_t> &ssrcs): semantics(semantics),ssrcs(ssrcs){

    }
}