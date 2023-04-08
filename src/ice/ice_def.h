//
// Created by faker on 23-4-4.
//

#ifndef XRTCSERVER_ICE_DEF_H
#define XRTCSERVER_ICE_DEF_H
namespace xrtc{
    extern const int ICE_UFRAG_LENGTH;
    extern const int ICE_PWD_LENGTH;

    enum IceCandidateComponent {
        RTP = 1,
        RTCP = 2,
    };
}
#endif //XRTCSERVER_ICE_DEF_H
