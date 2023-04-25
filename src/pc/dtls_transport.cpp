//
// Created by faker on 23-4-25.
//

#include "dtls_transport.h"
#include "rtc_base/logging.h"

namespace xrtc{
    const size_t k_dtls_record_header_len = 13;
    bool is_dtls_packet(const char* buf,size_t len){
        const uint8_t* u = reinterpret_cast<const uint8_t*>(buf);
        return len >= k_dtls_record_header_len && (u[0] > 19 && u[0] < 64) ;
    }
    bool is_dtls_client_hello_packet(const char* buf,size_t len){
        if(!is_dtls_packet(buf,len)){
            return false;
        }
        const uint8_t* u = reinterpret_cast<const uint8_t*>(buf);
        return len >= 17 && (u[0] == 22 && u[13] == 1) ;
    }
    DtlsTransport::DtlsTransport(IceTransportChannel *ice_channel):
            _ice_channel(ice_channel){
        _ice_channel->signal_read_packet.connect(this,&DtlsTransport::_on_read_packet);
    }
    void DtlsTransport::_on_read_packet(IceTransportChannel* /*channel*/, const char *buf, size_t len,int64_t ts) {
        switch(_dtls_state){
            case DtlsTransportState::k_new:
                if(_dtls){
                    RTC_LOG(LS_INFO) << to_string() << ": Received packet before DTLS started";
                }else{
                    RTC_LOG(LS_WARNING) << to_string() << ": Received packet before we know if "
                        << "we are doing DTLS or not";
                }
                if(is_dtls_client_hello_packet(buf,len)){
                    RTC_LOG(LS_INFO) << to_string() << ": Caching DTLS ClientHello packet until "
                        << "DTLS started";
                    _cached_client_hello.SetData(buf,len);
                    if(!_dtls && _local_certificate){
                        _setup_dtls();
                    }
                }else{
                    RTC_LOG(LS_WARNING) << to_string() << ": not a DTLS ClientHello packet, "
                        << "dropping";
                }
                break;
        }
    }

    bool DtlsTransport::_setup_dtls(){
        return false;
    }
    std::string DtlsTransport::to_string(){
        std::stringstream ss;
        absl::string_view RECEIVING[2] = {"-","R"};
        absl::string_view WRITABLE[2] = {"-","W"};
        ss << "DtlsTransport[" << transport_name() << ":" << (int)component() << "]"
           << RECEIVING[_receving] << WRITABLE[_writable];
        return ss.str();

    }
    DtlsTransport::~DtlsTransport() {

    }
}