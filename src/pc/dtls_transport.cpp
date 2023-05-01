//
// Created by faker on 23-4-25.
//
#include "dtls_transport.h"
#include "rtc_base/logging.h"
#include <api/crypto/crypto_options.h>

namespace xrtc {
    const size_t k_dtls_record_header_len = 13;
    const size_t k_max_dtls_packet_len = 2048;
    const size_t k_max_pending_packets = 2;
    const size_t k_min_rtp_packet_len = 12;

    bool is_dtls_packet(const char* buf, size_t len) {
        const uint8_t* u = reinterpret_cast<const uint8_t*>(buf);
        return len >= k_dtls_record_header_len && (u[0] > 19 && u[0] < 64);
    }

    bool is_dtls_client_hello_packet(const char* buf, size_t len) {
        if (!is_dtls_packet(buf, len)) {
            return false;
        }

        const uint8_t* u = reinterpret_cast<const uint8_t*>(buf);
        return len > 17 && (u[0] == 22 && u[13] == 1);
    }

    bool is_rtp_packet(const char* buf, size_t len) {
        const uint8_t* u = reinterpret_cast<const uint8_t*>(buf);
        return len >= k_min_rtp_packet_len && ((u[0] & 0xC0) == 0x80);
    }


    StreamInterfaceChannel::StreamInterfaceChannel(IceTransportChannel *ice_channel) :
            _ice_channel(ice_channel),
            _packets(k_max_pending_packets, k_max_dtls_packet_len) {

    }

    // 一旦客户端有数据包发送过来
    bool StreamInterfaceChannel::on_received_packet(const char *data, size_t size) {
        if (_packets.size() > 0) {
            RTC_LOG(LS_INFO) << ": Pakcet already in buffer queue";
        }
        if (!_packets.WriteBack(data, size, NULL)) {
            RTC_LOG(LS_WARNING) << ": Failed to write packet to buffer queue";
        }
        // 发送信号 被SSLadapter捕捉
        SignalEvent(this, rtc::SE_READ, 0);
        return true;
    }

    rtc::StreamState StreamInterfaceChannel::GetState() const {
        return _state;
    }
    rtc::StreamResult StreamInterfaceChannel::Read(void *buffer, size_t buffer_len, size_t *read, int * /*error*/) {
        if (_state == rtc::SS_CLOSED) {
            return rtc::SR_EOS;
        }

        if (_state == rtc::SS_OPENING) {
            return rtc::SR_BLOCK;
        }

        if (!_packets.ReadFront(buffer, buffer_len, read)) {
            return rtc::SR_BLOCK;
        }

        return rtc::SR_SUCCESS;

    }
    rtc::StreamResult StreamInterfaceChannel::Write(const void *data, size_t data_len, size_t *written, int */*error*/) {
        _ice_channel->send_packet((const char *) data, data_len);
        if (written) {
            *written = data_len;
        }
        return rtc::SR_SUCCESS;
    }
    void StreamInterfaceChannel::Close() {
        _state = rtc::SS_CLOSED;
        _packets.Clear();
    }


    DtlsTransport::DtlsTransport(IceTransportChannel *ice_channel) :
            _ice_channel(ice_channel) {
        _ice_channel->signal_read_packet.connect(this, &DtlsTransport::_on_read_packet);
        _ice_channel->signal_writable_state.connect(this, &DtlsTransport::_on_writable_state);
        _ice_channel->signal_receiving_state.connect(this, &DtlsTransport::_on_receiving_state);

        webrtc::CryptoOptions crypto_options;
        _srtp_ciphers = crypto_options.GetSupportedDtlsSrtpCryptoSuites();

    }


    DtlsTransport::~DtlsTransport() {

    }




    void DtlsTransport::_on_read_packet(IceTransportChannel * /*channel*/, const char *buf, size_t len, int64_t ts) {
        switch (_dtls_state) {
            case DtlsTransportState::k_new:
                if (_dtls) {
                    RTC_LOG(LS_INFO) << to_string() << ": Received packet before DTLS started.";
                } else {
                    RTC_LOG(LS_WARNING) << to_string() << ": Received packet before we know if "
                                        << "we are doing DTLS or not";
                }

                if (is_dtls_client_hello_packet(buf, len)) {
                    RTC_LOG(LS_INFO) << to_string() << ": Caching DTLS ClientHello packet until "
                                     << "DTLS started";
                    _cached_client_hello.SetData(buf, len);

                    if (!_dtls && _local_certificate) {
                        _setup_dtls();
                    }

                } else {
                    RTC_LOG(LS_WARNING) << to_string() << ": Not a DTLS ClientHello packet, "
                                        << "dropping";
                }

                break;
            case DtlsTransportState::k_connecting:
            case DtlsTransportState::k_connected:
                if (is_dtls_packet(buf, len)) { // Dtls包
                    if (!_handle_dtls_packet(buf, len)) {
                        RTC_LOG(LS_WARNING) << to_string() << ": handle DTLS packet failed";
                        return;
                    }
                } else { // RTP/RTCP包
                    if (_dtls_state != DtlsTransportState::k_connected) {
                        RTC_LOG(LS_WARNING) << to_string() << ": Received non DTLS packet "
                                            << "before DTLS complete";
                        return;
                    }

                    if (!is_rtp_packet(buf, len)) {
                        RTC_LOG(LS_WARNING) << to_string() << ": Received unexpected non "
                                            << "DTLS packet";
                        return;
                    }

                    RTC_LOG(LS_INFO) << "==============rtp received: " << len;
                    signal_read_packet(this, buf, len, ts);
                }

                break;
            default:
                break;
        }

    }


    void DtlsTransport::_on_writable_state(IceTransportChannel *channel) {
        RTC_LOG(LS_INFO) << to_string() << ": IceTransportChannel writable changed to " << channel->writable();
        if (!_dtls_active) {
            _set_writable_state(channel->writable());
            return;
        }
        switch (_dtls_state) {
//           还未启动
            case DtlsTransportState::k_new:
                _maybe_start_dtls();
                break;
//           重新改变状态 有可能之前连上了 中途断开了
            case DtlsTransportState::k_connected:
                _set_writable_state(channel->writable());
                break;
            default:
                break;
        }
    }


    void DtlsTransport::_on_receiving_state(IceTransportChannel *channel) {
        _set_receiving(channel->receiving());
    }

    void DtlsTransport::_set_receiving(bool receiving) {
        if (_receiving == receiving) {
            return;
        }

        RTC_LOG(LS_INFO) << to_string() << ": Change receiving to " << receiving;
        _receiving = receiving;
        signal_receiving_state(this);
    }


    bool DtlsTransport::set_local_certificate(rtc::RTCCertificate *cert) {
        if (_dtls_active) {
            if (cert == _local_certificate) {
                RTC_LOG(LS_INFO) << to_string() << ": Ignoring identical DTLS cert";
                return true;
            } else {
//               不允许重新改变证书
                RTC_LOG(LS_WARNING) << to_string() << ": Cannot change cert in this state";
                return false;
            }
        }
        if (cert) {
            _local_certificate = cert;
            _dtls_active = true;
        }
        return true;
    }

//   收到answersdp的时候才会去设置 有可能hello packet先到
    bool
    DtlsTransport::set_remote_fingerprint(const std::string &digest_alg, const uint8_t *digest, size_t digest_len) {
        rtc::Buffer remote_fingerprint_value(digest, digest_len);
        if (_dtls_active && _remote_fingerprint_value == remote_fingerprint_value &&
            !digest_alg.empty()) {
            RTC_LOG(LS_INFO) << to_string() << ": Ignoring identical remote fingerprint";
            return true;
        }
        if (digest_alg.empty()) {
            RTC_LOG(LS_WARNING) << to_string() << ": Other sides not support DTLS";
//           如果对方不支持dtls
            _dtls_active = false;
            return false;
        }
        if (!_dtls_active) {
            RTC_LOG(LS_WARNING) << to_string() << ": Cannot set remote fingerprint in this state";
            return false;
        }
        bool fingerprint_change = _remote_fingerprint_value.size() > 0u;

        _remote_fingerprint_value = std::move(remote_fingerprint_value);
        _remote_fingerprint_alg = digest_alg;
//       Client hello packet 先到 answer sdp后到
        if (_dtls && !fingerprint_change) {
            rtc::SSLPeerCertificateDigestError err;
            if (!_dtls->SetPeerCertificateDigest(digest_alg, (const unsigned char *) digest, digest_len, &err)) {
                RTC_LOG(WARNING) << to_string() << ": Failed to set peer certificate digest";
                _set_dtls_state(DtlsTransportState::k_failed);
//               如果是验证错误 还是true
                return err == rtc::SSLPeerCertificateDigestError::VERIFICATION_FAILED;
            }
            return true;
        }
//       指纹发生变化
        if (_dtls && fingerprint_change) {
            _dtls.reset(nullptr);
            _set_dtls_state(DtlsTransportState::k_new);
            _set_writable_state(false);
        }
        if (!_setup_dtls()) {
            RTC_LOG(LS_WARNING) << to_string() << ": Failed to setup DTLS";
            _set_dtls_state(DtlsTransportState::k_failed);
            return false;
        }

        return true;


    }

    void DtlsTransport::_set_dtls_state(DtlsTransportState state) {

        if (_dtls_state == state) {
            return;
        }
        RTC_LOG(LS_INFO) << to_string() << ": Change dtls state from " << _dtls_state << " to " << state;
        _dtls_state = state;
        signal_dtls_state(this, state);

    }
    void DtlsTransport::_set_writable_state(bool writable) {

        if (_writable == writable) {
            return;
        }
        RTC_LOG(LS_INFO) << to_string() << ": set DTLS writable to " << writable;
        _writable = writable;
        signal_writable_state(this);
    }
    bool DtlsTransport::_setup_dtls() {
        auto downward = std::make_unique<StreamInterfaceChannel>(_ice_channel);
        StreamInterfaceChannel *downward_ptr = downward.get();
        // 这里和streaminterfacechannel建立了联系 adapter内部连接了streaminterfacechannel的信号
        _dtls = rtc::SSLStreamAdapter::Create(std::move(downward));

        if (!_dtls) {
            RTC_LOG(LS_WARNING) << to_string() << ": Failed to create SSLStreamAdapter";
            return false;
        }
        _downward = downward_ptr;
        _dtls->SetIdentity(_local_certificate->identity()->Clone());
        _dtls->SetMode(rtc::SSL_MODE_DTLS);
        _dtls->SetMaxProtocolVersion(rtc::SSL_PROTOCOL_DTLS_12);
        _dtls->SetServerRole(rtc::SSL_SERVER);
        _dtls->SignalEvent.connect(this, &DtlsTransport::_on_dtls_event);
        _dtls->SignalSSLHandshakeError.connect(this,
                                               &DtlsTransport::_on_dtls_handshake_error);
//       finerprint是通过sdp交换拿到的 也就是answersdp
        if (_remote_fingerprint_value.size() && !_dtls->SetPeerCertificateDigest(
                _remote_fingerprint_alg,
                _remote_fingerprint_value.data(),
                _remote_fingerprint_value.size()
        )) {
            RTC_LOG(LS_WARNING) << to_string() << ": Failed to set remote fingerprint";
            return false;
        }
        if (!_srtp_ciphers.empty()) {
            if (!_dtls->SetDtlsSrtpCryptoSuites(_srtp_ciphers)) {
                RTC_LOG(LS_WARNING) << to_string() << ": Failed to set DTLS-SRTP crypto suites";
                return false;
            }
        } else {
            RTC_LOG(LS_WARNING) << to_string() << ": Not using DTLS-SRTP";
        }

        RTC_LOG(LS_INFO) << to_string() << ": Setup DTLS complete";
        _maybe_start_dtls();

        return true;
    }

    void DtlsTransport::_on_dtls_event(rtc::StreamInterface */*dtls*/, int sig, int error) {
        if (sig & rtc::SE_OPEN) {
            RTC_LOG(LS_INFO) << to_string() << ": DTLS handshake complete.";
            _set_writable_state(true);
            _set_dtls_state(DtlsTransportState::k_connected);
        }

        if (sig & rtc::SE_READ) {
            char buf[k_max_dtls_packet_len];
            size_t read;
            int read_error;
            rtc::StreamResult ret;
            // 因为一个数据包可能会包含多个DTLS record，需要循环读取
            do {
                ret = _dtls->Read(buf, sizeof(buf), &read, &read_error);
                if (ret == rtc::SR_SUCCESS) {
                } else if (ret == rtc::SR_EOS) {
                    RTC_LOG(LS_INFO) << to_string() << ": DTLS transport closed by remote.";
                    _set_writable_state(false);
                    _set_dtls_state(DtlsTransportState::k_closed);
                    signal_closed(this);
                } else if (ret == rtc::SR_ERROR) {
                    RTC_LOG(LS_WARNING) << to_string() << ": Closed DTLS transport by remote "
                                        << "with error, code=" << read_error;
                    _set_writable_state(false);
                    _set_dtls_state(DtlsTransportState::k_failed);
                    signal_closed(this);
                }

            } while (ret == rtc::SR_SUCCESS);
        }

        if (sig & rtc::SE_CLOSE) {
            if (!error) {
                RTC_LOG(LS_INFO) << to_string() << ": DTLS transport closed";
                _set_writable_state(false);
                _set_dtls_state(DtlsTransportState::k_closed);
            } else {
                RTC_LOG(LS_INFO) << to_string() << ": DTLS transport closed with error, "
                                 << "code=" << error;
                _set_writable_state(false);
                _set_dtls_state(DtlsTransportState::k_failed);
            }
        }
    }

    void DtlsTransport::_on_dtls_handshake_error(rtc::SSLHandshakeError err) {
        RTC_LOG(LS_WARNING) << to_string() << ": DTLS handshake error=" << (int) err;
    }

    void DtlsTransport::_maybe_start_dtls() {
//       ice_channel writable需要一定时间
        if (_dtls && _ice_channel->writable()) {
            if (_dtls->StartSSL()) {
                RTC_LOG(LS_WARNING) << ": Failed to StartSSL.";
                _set_dtls_state(DtlsTransportState::k_failed);
                return;
            }
            RTC_LOG(LS_INFO) << to_string() << ": Started DTLS.";;
            _set_dtls_state(DtlsTransportState::k_connecting);
            if (_cached_client_hello.size()) {
                if (!_handle_dtls_packet(_cached_client_hello.data<char>(), _cached_client_hello.size())) {
                    RTC_LOG(LS_WARNING) << to_string() << ": Handing dtls packet failed.";
                    _set_dtls_state(DtlsTransportState::k_failed);
                }
                _cached_client_hello.Clear();
            }
        }

    }

    bool DtlsTransport::_handle_dtls_packet(const char *data, size_t size) {
        const uint8_t *tmp_data = reinterpret_cast<const uint8_t *>(data);
//       data可能包含多分的数据
        size_t tmp_size = size;
        while (tmp_size > 0) {
            if (tmp_size < k_dtls_record_header_len) {
                return false;
            }
            size_t record_len = (tmp_data[11] << 8)| tmp_data[12];
            if (record_len + k_dtls_record_header_len > tmp_size) {
                return false;
            }
            tmp_data += k_dtls_record_header_len + record_len;
            tmp_size -= k_dtls_record_header_len + record_len;
        }
//       传给streaminterfacechannel来处理
        return _downward->on_received_packet(data, size);
    }
    std::string DtlsTransport::to_string() {
        std::stringstream ss;
        absl::string_view RECEIVING[2] = {"-", "R"};
        absl::string_view WRITABLE[2] = {"-", "W"};

        ss << "DtlsTransport[" << transport_name() << "|"
           << (int) component() << "|"
           << RECEIVING[_receiving] << "|"
           << WRITABLE[_writable] << "]";
        return ss.str();
    }

    bool DtlsTransport::get_srtp_crypto_suite(int* selected_crypto_suite) {
        if(_dtls_state != DtlsTransportState::k_connected){
            return false;
        }
        return _dtls->GetDtlsSrtpCryptoSuite(selected_crypto_suite);
    }

    bool DtlsTransport::export_keying_material(const std::string &label, const uint8_t *context, size_t context_len,
                                               bool use_context, uint8_t *result, size_t result_len) {
        return _dtls.get() ? _dtls->ExportKeyingMaterial(label, context, context_len, use_context, result, result_len)
                           : false;
    }

}

































