//
// Created by faker on 23-4-29.
//

#include "dtls_srtp_transport.h"
#include "rtc_base/logging.h"
#include "module/rtc_rtcp/rtp_utils.h"
#include "rtc_base/copy_on_write_buffer.h"

namespace xrtc {
    // rtc5764
    static char k_dtls_srtp_exporter_label[] = "EXTRACTOR-dtls_srtp";

    DtlsSrtpTransport::DtlsSrtpTransport(const std::string &transport_name,
                                         bool rtcp_mux_enabled) : SrtpTransport(rtcp_mux_enabled),
                                                                  _transport_name(transport_name) {


    }

    void DtlsSrtpTransport::set_dtls_transports(DtlsTransport* rtp_dtls_transport,
                                                DtlsTransport* rtcp_dtls_transport)
    {
        _rtp_dtls_transport = rtp_dtls_transport;
        _rtcp_dtls_transport = rtcp_dtls_transport;

        if (_rtp_dtls_transport) {
            _rtp_dtls_transport->signal_dtls_state.connect(this,
                                                           &DtlsSrtpTransport::_on_dtls_state);
            _rtp_dtls_transport->signal_read_packet.connect(this,&DtlsSrtpTransport::_on_read_packet);
        }

        _maybe_setup_dtls_srtp();
    }
    void DtlsSrtpTransport::_on_read_packet(DtlsTransport* /*dtls*/,const char* data,size_t len,int64_t ts){
        auto array_new = rtc::MakeArrayView(data,len);
        // 判断rtp还是rtcp包
        RtpPacketType packet_type = infer_rtp_packet_type(array_new);

        if(packet_type == RtpPacketType::k_unknown){
            return ;
        }
        rtc::CopyOnWriteBuffer packet(data,len);
        if(packet_type == RtpPacketType::k_rtcp){
            _on_rtcp_packet_received(std::move(packet),ts);
        }else{
            _on_rtp_packet_received(std::move(packet),ts);
        }

    }

    void DtlsSrtpTransport::_on_rtcp_packet_received(rtc::CopyOnWriteBuffer packet, int64_t ts) {
        if(!is_srtp_active()){
            RTC_LOG(LS_WARNING) << "Inactive SRTP transport received a rtp packet, drop it";
            return;
        }
        char* data = packet.data<char>();
        int len = packet.size();
        if(!unprotect_rtcp(data,len,&len)){
            int type = 0;
            get_rtcp_type(data,len,&type);
                RTC_LOG(LS_WARNING) << "Failed to unprotect rtcp packet: " <<
                                    ", size=" << len <<
                                    ", type=" << type;
            return;
        }
        packet.SetSize(len);
        signal_rtcp_packet_received(this,&packet,ts);

    }


    void DtlsSrtpTransport::_on_rtp_packet_received(rtc::CopyOnWriteBuffer packet, int64_t ts) {
        if(!is_srtp_active()){
            RTC_LOG(LS_WARNING) << "Inactive SRTP transport received a rtp packet, drop it";
            return;
        }
        char* data = packet.data<char>();
        int len = packet.size();
        if(!unprotect_rtp(data,len,&len)){
            const int k_fail_log = 100;
            if(_unprotect_fail_count % k_fail_log == 0){
                RTC_LOG(LS_WARNING) << "Failed to unprotect rtp packet: " <<
                    ", size=" << len <<
                    ", seqnum=" << parse_rtp_sequence_number(packet) <<
                    ", ssrc=" << parse_rtp_ssrc(packet) <<
                    ", unprotect_fail_count=" << _unprotect_fail_count;

            }
            _unprotect_fail_count++;
            return;
        }
        packet.SetSize(len);
        signal_rtp_packet_received(this,&packet,ts);

    }

    void DtlsSrtpTransport::_on_dtls_state(DtlsTransport* /*dtls*/,
                                           DtlsTransportState state)
    {
        if (state != DtlsTransportState::k_connected) {
            reset_params();
            return;
        }
        _maybe_setup_dtls_srtp();
    }
    bool DtlsSrtpTransport::is_dtls_writable() {
        // 根据是否复用
        auto rtcp_transport = _rtcp_mux_enabled ? nullptr : _rtcp_dtls_transport;
        return _rtp_dtls_transport && _rtp_dtls_transport->writable() &&
               (!rtcp_transport || rtcp_transport->writable());

    }

    void DtlsSrtpTransport::_maybe_setup_dtls_srtp() {
        if (is_srtp_active() || !is_dtls_writable()) {
            return;
        }

        _setup_dtls_srtp();
    }


    void DtlsSrtpTransport::_setup_dtls_srtp() {
        std::vector<int> send_extension_ids;
        std::vector<int> recv_extension_ids;

        int selected_crypto_suite;
        rtc::ZeroOnFreeBuffer<unsigned char> send_key;
        rtc::ZeroOnFreeBuffer<unsigned char> recv_key;

        if (!_extract_params(_rtp_dtls_transport, &selected_crypto_suite,
                             &send_key, &recv_key) ||
            !set_rtp_params(selected_crypto_suite,
                            &send_key[0], send_key.size(), send_extension_ids,
                            selected_crypto_suite,
                            &recv_key[0], recv_key.size(), recv_extension_ids))
        {
            RTC_LOG(LS_WARNING) << "DTLS-SRTP rtp param install failed";
        }
    }


    bool DtlsSrtpTransport::_extract_params(DtlsTransport *dtls_transport,
                                            int *selected_crypto_suite,
                                            rtc::ZeroOnFreeBuffer<unsigned char> *send_key,
                                            rtc::ZeroOnFreeBuffer<unsigned char> *recv_key) {
        if (!dtls_transport || !dtls_transport->is_dtls_active()) {
            return false;
        }

        if (!dtls_transport->get_srtp_crypto_suite(selected_crypto_suite)) {
            RTC_LOG(LS_WARNING) << "No selected crypto suite";
            return false;
        }

        RTC_LOG(LS_INFO) << "Extract DTLS-SRTP key from transport " << _transport_name;

        int key_len;
        int salt_len;
        if (!rtc::GetSrtpKeyAndSaltLengths(*selected_crypto_suite, &key_len, &salt_len)) {
            RTC_LOG(LS_WARNING) << "Unknown DTLS-SRTP crypto suite " << *selected_crypto_suite;
            return false;
        }

        rtc::ZeroOnFreeBuffer<unsigned char> dtls_buffer(key_len * 2 + salt_len * 2);
        if (!dtls_transport->export_keying_material(k_dtls_srtp_exporter_label,
                                                    NULL, 0, false, &dtls_buffer[0], dtls_buffer.size()))
        {
            RTC_LOG(LS_WARNING) << "Extracting DTLS-SRTP param failed";
            return false;
        }

        rtc::ZeroOnFreeBuffer<unsigned char> client_write_key(key_len + salt_len);
        rtc::ZeroOnFreeBuffer<unsigned char> server_write_key(key_len + salt_len);
        size_t offset = 0;
        memcpy(&client_write_key[0], &dtls_buffer[offset], key_len);
        offset += key_len;
        memcpy(&server_write_key[0], &dtls_buffer[offset], key_len);
        offset += key_len;
        memcpy(&client_write_key[key_len], &dtls_buffer[offset], salt_len);
        offset += salt_len;
        memcpy(&server_write_key[key_len], &dtls_buffer[offset], salt_len);

        *send_key = std::move(server_write_key);
        *recv_key = std::move(client_write_key);

        return true;


    }

    int DtlsSrtpTransport::send_rtp(const char *data, size_t len) {
        return -1;
    }

}
