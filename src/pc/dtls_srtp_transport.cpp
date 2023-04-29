//
// Created by faker on 23-4-29.
//

#include "dtls_srtp_transport.h"
#include "rtc_base/logging.h"

namespace xrtc{
    // rtc5764
    static char k_dtls_srtp_exporter_label[] = "EXTRACTOR-dtls_srtp";
    DtlsSrtpTransport::DtlsSrtpTransport(const std::string &transport_name,
                                         bool rtcp_mux_enabled): SrtpTransport(rtcp_mux_enabled),_transport_name(transport_name){


    }

    void DtlsSrtpTransport::set_dtls_transports(DtlsTransport* rtp_dtls_transport,
                                                DtlsTransport* rtcp_dtls_transport) {

        _rtcp_dtls_transport = rtcp_dtls_transport;
        _rtp_dtls_transport = rtp_dtls_transport;
    }
    bool DtlsSrtpTransport::_extract_params(DtlsTransport* dtls_transport,
                                            int* selected_crypto_suite,
                                            rtc::ZeroOnFreeBuffer<unsigned char>* send_key,
                                            rtc::ZeroOnFreeBuffer<unsigned char>* recv_key){
        if(!dtls_transport || !dtls_transport->is_dtls_active()){
            return false;
        }
        if(!dtls_transport->get_srtp_crypto_suite(selected_crypto_suite)){
            RTC_LOG(LS_WARNING) << "No selected srtp crypto suite";
            return false;
        }
        RTC_LOG(LS_INFO) << "Extract DTLS_SRTP key from transport " << _transport_name;
        int key_len;
        int salt_len;
        if(!rtc::GetSrtpKeyAndSaltLengths(*selected_crypto_suite,&key_len,&salt_len)){
            RTC_LOG(LS_WARNING) << "Unknown srtp crypto suite: " << *selected_crypto_suite;
            return false;
        }

        rtc::ZeroOnFreeBuffer<unsigned char> dtls_buffer(key_len * 2 + salt_len * 2);

        if(!dtls_transport->export_keying_material(k_dtls_srtp_exporter_label,
                                                   NULL,0,false,&dtls_buffer[0],dtls_buffer.size())){
            RTC_LOG(LS_WARNING) << "Extracting DTLS_SRTP param failed";
            return false;
        }

        rtc::ZeroOnFreeBuffer<unsigned char> client_write_key(key_len + salt_len);
        rtc::ZeroOnFreeBuffer<unsigned char> server_write_key(key_len + salt_len);
        size_t offset = 0;
        memcpy(&client_write_key[0],&dtls_buffer[offset],key_len);
        offset += key_len;
        memcpy(&server_write_key[0],&dtls_buffer[offset],key_len);
        offset += key_len;
        memcpy(&client_write_key[key_len],&dtls_buffer[offset],salt_len);
        offset += salt_len;
        memcpy(&server_write_key[key_len],&dtls_buffer[offset],salt_len);

        *send_key = std::move(server_write_key);
        *recv_key = std::move(client_write_key);

        return true;

    }
}
