//
// Created by faker on 23-4-2.
//

#ifndef XRTCSERVER_RTC_STREAM_H
#define XRTCSERVER_RTC_STREAM_H

#include <cstdint>
#include <string>
#include <memory>
#include "base/event_loop.h"
#include "pc/peer_connection.h"
#include "ice/port_allocator.h"
#include <rtc_base/rtc_certificate.h>
namespace xrtc{
    class RtcStream;
    enum class RtcStreamType{
        k_push,
        k_pull
    };
    class RtcStreamListener{
    public:
        virtual void on_connection_state(RtcStream* stream,PeerConnectionState state) = 0;
        virtual void on_rtp_packet_received(RtcStream* stream,const char* data,size_t len) = 0;
        virtual void on_rtcp_packet_received(RtcStream* stream,const char* data,size_t len) = 0;
    };



    class RtcStream : public sigslot::has_slots<>{
    public:
        RtcStream(EventLoop* _el,PortAllocator* allocator, uint64_t,const std::string& stream_name,
                  bool audiom,bool video,uint32_t log_id);
        virtual ~RtcStream();
        int start(rtc::RTCCertificate* certificate);
        virtual std::string create_offer() = 0;
        int set_remote_sdp(const std::string& sdp);
        void register_listener(RtcStreamListener* listener){
            _listener = listener;
        }
        virtual RtcStreamType stream_type() = 0;
        uint64_t get_uid(){
            return uid;
        }
        const std::string get_stream_name(){
            return stream_name;
        }
        std::string to_string();
    private:
        void _on_connection_state(PeerConnection *, PeerConnectionState state);
        void _on_rtp_packet_received(PeerConnection *, rtc::CopyOnWriteBuffer *packet, int64_t);
        void _on_rtcp_packet_received(PeerConnection *, rtc::CopyOnWriteBuffer *packet, int64_t);
    protected:
        EventLoop* el;
        uint64_t uid;
        std::string stream_name;
        bool audio;
        bool video;
        uint32_t log_id;
        PeerConnection* pc;
        friend class RtcStreamManager;
        PeerConnectionState _state = PeerConnectionState::k_new;
        RtcStreamListener* _listener = nullptr;

    };

}


#endif //XRTCSERVER_RTC_STREAM_H
