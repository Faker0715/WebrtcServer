//
// Created by faker on 23-4-25.
//

#include "dtls_transport.h"
#include "rtc_base/logging.h"

namespace xrtc{

    DtlsTransport::DtlsTransport(IceTransportChannel *ice_channel):
            _ice_channel(ice_channel){
        _ice_channel->signal_read_packet.connect(this,&DtlsTransport::_on_read_packet);
    }
    void DtlsTransport::_on_read_packet(IceTransportChannel* /*channel*/, const char *buf, size_t len,int64_t ts) {
        RTC_LOG(LS_INFO) << "============DTLS packet: " << len;
    }

    DtlsTransport::~DtlsTransport() {

    }
}