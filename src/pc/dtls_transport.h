//
// Created by faker on 23-4-25.
//

#ifndef XRTCSERVER_DTLS_TRANSPORT_H
#define XRTCSERVER_DTLS_TRANSPORT_H

#include "ice/ice_transport_channel.h"
#include <memory>
#include <rtc_base/ssl_stream_adapter.h>
#include <rtc_base/buffer_queue.h>
#include <rtc_base/rtc_certificate.h>

namespace xrtc{
    enum class DtlsTransportState{
        k_new,
        k_connecting,
        k_connected,
        k_closed,
        k_failed,
        k_num_values
    };
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
        std::string to_string();
    private:
        void _on_read_packet(IceTransportChannel*, const char *buf, size_t len, int64_t ts);
        bool _setup_dtls();
    private:
        IceTransportChannel* _ice_channel;
        DtlsTransportState _dtls_state = DtlsTransportState::k_new;
        bool _receving = false;
        bool _writable = false;
        std::unique_ptr<rtc::SSLStreamAdapter> _dtls;
        rtc::Buffer _cached_client_hello;
        rtc::RTCCertificate* _local_certificate = nullptr;

    };

}


#endif //XRTCSERVER_DTLS_TRANSPORT_H
