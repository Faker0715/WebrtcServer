//
// Created by faker on 23-4-29.
//

#ifndef XRTCSERVER_DTLS_SRTP_TRANSPORT_H
#define XRTCSERVER_DTLS_SRTP_TRANSPORT_H

#include <string>
#include <cstdint>
#include "srtp_transport.h"
#include "dtls_transport.h"
#include "rtc_base/copy_on_write_buffer.h"

namespace xrtc {

    class DtlsTransport;

    class DtlsSrtpTransport : public SrtpTransport {
    public:
        DtlsSrtpTransport(const std::string& transport_name, bool rtcp_mux_enabled);

        void set_dtls_transports(DtlsTransport* rtp_dtls_transport,
                                 DtlsTransport* rtcp_dtls_transport);
        bool is_dtls_writable();
        sigslot::signal3<DtlsSrtpTransport* ,rtc::CopyOnWriteBuffer*,int64_t>
            signal_rtp_packet_received;

    private:
        bool _extract_params(DtlsTransport* dtls_transport,
                             int* selected_crypto_suite,
                             rtc::ZeroOnFreeBuffer<unsigned char>* send_key,
                             rtc::ZeroOnFreeBuffer<unsigned char>* recv_key);
        void _maybe_setup_dtls_srtp();
        void _setup_dtls_srtp();
        void _on_dtls_state(DtlsTransport* dtls, DtlsTransportState state);
        void _on_read_packet(DtlsTransport *dtls, const char *data, size_t len, int64_t);
        void _on_rtp_packet_received(rtc::CopyOnWriteBuffer packet, int64_t ts);
    private:
        std::string _transport_name;
        DtlsTransport* _rtp_dtls_transport = nullptr;
        DtlsTransport* _rtcp_dtls_transport = nullptr;
        int _unprotect_fail_count = 0;

    };

}



#endif //XRTCSERVER_DTLS_SRTP_TRANSPORT_H
