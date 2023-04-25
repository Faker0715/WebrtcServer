//
// Created by faker on 23-4-25.
//

#ifndef XRTCSERVER_ICE_CONNECTION_INFO_H
#define XRTCSERVER_ICE_CONNECTION_INFO_H

namespace xrtc{
    enum class IceCandidatePairState{
        WAITING, // 连通性检查尚未开始
        IN_PROGRESS, // 连通性检查正在进行
        SUCCEEDED, // 连通性检查成功
        FAILED, // 连通性检查失败
    };
}


#endif //XRTCSERVER_ICE_CONNECTION_INFO_H
