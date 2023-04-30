//
// Created by faker on 23-4-29.
//

#ifndef XRTCSERVER_DTLS_SRTP_TRANSPORT_H
#define XRTCSERVER_DTLS_SRTP_TRANSPORT_H

#include <string>
#include "srtp_transport.h"
#include "dtls_transport.h"

namespace xrtc {

    class DtlsTransport;

    class DtlsSrtpTransport : public SrtpTransport {
    public:
        DtlsSrtpTransport(const std::string& transport_name, bool rtcp_mux_enabled);

        void set_dtls_transports(DtlsTransport* rtp_dtls_transport,
                                 DtlsTransport* rtcp_dtls_transport);
        bool is_dtls_writable();

    private:
        bool _extract_params(DtlsTransport* dtls_transport,
                             int* selected_crypto_suite,
                             rtc::ZeroOnFreeBuffer<unsigned char>* send_key,
                             rtc::ZeroOnFreeBuffer<unsigned char>* recv_key);
        void _maybe_setup_dtls_srtp();
        void _setup_dtls_srtp();
        void _on_dtls_state(DtlsTransport* dtls, DtlsTransportState state);

    private:
        std::string _transport_name;
        DtlsTransport* _rtp_dtls_transport = nullptr;
        DtlsTransport* _rtcp_dtls_transport = nullptr;
    };

}



#endif //XRTCSERVER_DTLS_SRTP_TRANSPORT_H
