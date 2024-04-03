#ifndef  __XRTCSERVER_PC_DTLS_SRTP_TRANSPORT_H_
#define  __XRTCSERVER_PC_DTLS_SRTP_TRANSPORT_H_

#include <string>

#include <rtc_base/buffer.h>
#include <rtc_base/copy_on_write_buffer.h>

#include "pc/srtp_transport.h"

namespace xrtc {

class DtlsTransport;
enum class DtlsTransportState;

class DtlsSrtpTransport : public SrtpTransport {
public:
    DtlsSrtpTransport(const std::string& transport_name, bool rtcp_mux_enabled);
    
    void set_dtls_transports(DtlsTransport* rtp_dtls_transport,
            DtlsTransport* rtcp_dtls_transport);
    bool IsDtlsWritable();
    const std::string& transport_name() { return transport_name_; }
    int SendRtp(const char* data, size_t len);
    int SendRtcp(const char* data, size_t len);

    sigslot::signal3<DtlsSrtpTransport*, rtc::CopyOnWriteBuffer*, int64_t>
        SignalRtpPacketReceived;
    sigslot::signal3<DtlsSrtpTransport*, rtc::CopyOnWriteBuffer*, int64_t>
        SignalRtcpPacketReceived;

private:
    bool ExtractParams(DtlsTransport* dtls_transport,
            int* selected_crypto_suite,
            rtc::ZeroOnFreeBuffer<unsigned char>* send_key,
            rtc::ZeroOnFreeBuffer<unsigned char>* recv_key);
    void MaybeSetupDtlsSrtp();
    void SetupDtlsSrtp();
    void OnDtlsState(DtlsTransport* dtls, DtlsTransportState state);
    void OnReadPacket(DtlsTransport* dtls, const char* data, size_t len, int64_t ts);
    void OnRtpPacketReceived(rtc::CopyOnWriteBuffer packet, int64_t ts);
    void OnRtcpPacketReceived(rtc::CopyOnWriteBuffer packet, int64_t ts);

private:
    std::string transport_name_;
    DtlsTransport* rtp_dtls_transport_ = nullptr;
    DtlsTransport* rtcp_dtls_transport_ = nullptr;
    int unprotect_fail_count_ = 0;
    uint16_t last_send_seq_num_ = 0;
};

} // namespace xrtc

#endif  //__XRTCSERVER_PC_DTLS_SRTP_TRANSPORT_H_


