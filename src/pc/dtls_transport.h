//
// Created by faker on 23-4-25.
//

#ifndef XRTCSERVER_DTLS_TRANSPORT_H
#define XRTCSERVER_DTLS_TRANSPORT_H

#include "ice/ice_transport_channel.h"

namespace xrtc{
    class DtlsTransport: public sigslot::has_slots<>{
    public:
        DtlsTransport(IceTransportChannel* ice_channel);
        ~DtlsTransport();
        const std::string& transport_name(){
            return _ice_channel->transport_name();
        }
        IceCandidateComponent component(){
            return _ice_channel->component();
        }
    private:
        void _on_read_packet(IceTransportChannel*, const char *buf, size_t len, int64_t ts);
    private:
        IceTransportChannel* _ice_channel;
    };

}


#endif //XRTCSERVER_DTLS_TRANSPORT_H
