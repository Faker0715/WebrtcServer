//
// Created by faker on 23-4-29.
//

#ifndef XRTCSERVER_SRTP_TRANSPORT_H
#define XRTCSERVER_SRTP_TRANSPORT_H

namespace xrtc {
    class SrtpTransport {
    public:
        SrtpTransport(bool rtcp_mux_enabled);
        virtual ~SrtpTransport(){

        };
    private:
        bool _rtcp_mux_enabled;
    };
}


#endif //XRTCSERVER_SRTP_TRANSPORT_H
