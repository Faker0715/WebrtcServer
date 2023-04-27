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

namespace xrtc {
    class StreamInterfaceChannel : public rtc::StreamInterface {
    public:
        StreamInterfaceChannel(IceTransportChannel *ice_channel);

        rtc::StreamState GetState() const override;

        rtc::StreamResult Read(void *buffer,
                          size_t buffer_len,
                          size_t *read,
                          int *error) override;

        rtc::StreamResult Write(const void *data,
                           size_t data_len,
                           size_t *written,
                           int *error) override;

        void Close() override;

        bool on_received_packet(const char *string, size_t i);

    private:
        IceTransportChannel *_ice_channel;
        rtc::BufferQueue _packets;
        rtc::StreamState _state = rtc::SS_OPEN;
    };

    enum class DtlsTransportState {
        k_new,
        k_connecting,
        k_connected,
        k_closed,
        k_failed,
        k_num_values
    };

    class DtlsTransport : public sigslot::has_slots<> {
    public:
        DtlsTransport(IceTransportChannel *ice_channel);

        ~DtlsTransport();

        const std::string &transport_name() {
            return _ice_channel->transport_name();
        }

        IceCandidateComponent component() {
            return _ice_channel->component();
        }

        IceTransportChannel* ice_channel() {
            return _ice_channel;
        }


        std::string to_string();

        bool set_local_certificate(rtc::RTCCertificate* cert);
        bool set_remote_fingerprint(const std::string &digest_alg, const uint8_t * digest, size_t len);;
        DtlsTransportState dtls_state() {
            return _dtls_state;
        }
        sigslot::signal2<DtlsTransport*,DtlsTransportState> signal_dtls_state;
        sigslot::signal1<DtlsTransport*> signal_writable_state;
        sigslot::signal4<DtlsTransport*,const char*,size_t,int64_t> signal_read_packet;
        sigslot::signal1<DtlsTransport*> signal_closed;
        sigslot::signal1<DtlsTransport*> signal_receiving_state;
    private:
        void _on_read_packet(IceTransportChannel *, const char *buf, size_t len, int64_t ts);
        void  _maybe_start_dtls();
        bool _setup_dtls();
        void _set_dtls_state(DtlsTransportState state);
        void _set_writable_state(bool writable);
        bool _handle_dtls_packet(const char *data, size_t size);
        void _on_writable_state(IceTransportChannel* channel);
        void _on_receiving_state(IceTransportChannel* channel);
        void _on_dtls_event(rtc::StreamInterface* dtls,int sig,int error);
        void _on_dtls_handshake_error(rtc::SSLHandshakeError error);
        void _set_receiving(bool receiving);
    private:
        IceTransportChannel *_ice_channel;
        DtlsTransportState _dtls_state = DtlsTransportState::k_new;
        bool _receiving = false;
        bool _writable = false;
        std::unique_ptr<rtc::SSLStreamAdapter> _dtls;
        rtc::Buffer _cached_client_hello;
        rtc::RTCCertificate *_local_certificate = nullptr;
        StreamInterfaceChannel* _downward = nullptr;
        rtc::Buffer _remote_fingerprint_value;
        std::string _remote_fingerprint_alg;
        bool _dtls_active = false;
        std::vector<int> _srtp_ciphers;
    };

}


#endif //XRTCSERVER_DTLS_TRANSPORT_H
