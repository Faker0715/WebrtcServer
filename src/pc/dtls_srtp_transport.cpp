//
// Created by faker on 23-4-29.
//

#include "dtls_srtp_transport.h"
namespace xrtc{
    DtlsSrtpTransport::DtlsSrtpTransport(const std::string &transport_name,
                                         bool rtcp_mux_enabled): SrtpTransport(rtcp_mux_enabled),_transport_name(transport_name){


    }

    void DtlsSrtpTransport::set_dtls_transports(DtlsTransport* rtp_dtls_transport,
                                                DtlsTransport* rtcp_dtls_transport) {

        _rtcp_dtls_transport = rtcp_dtls_transport;
        _rtp_dtls_transport = rtp_dtls_transport;

    }
}
