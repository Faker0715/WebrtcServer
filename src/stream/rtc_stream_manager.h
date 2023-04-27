//
// Created by faker on 23-4-2.
//

#ifndef XRTCSERVER_RTC_STREAM_MANAGER_H
#define XRTCSERVER_RTC_STREAM_MANAGER_H

#include <string>
#include <unordered_map>
#include "base/event_loop.h"
#include "push_stream.h"
#include "ice/port_allocator.h"
#include <rtc_base/rtc_certificate.h>

namespace xrtc {
    class PushStream;

    class RtcStreamManager :public RtcStreamListener {
    public:
        RtcStreamManager(EventLoop *el);

        ~RtcStreamManager();

        int create_push_stream(uint64_t uid, const std::string &stream_name,
                               bool audio, bool video, uint32_t log_id,
                               rtc::RTCCertificate *certificate,
                               std::string &offer);
        int set_answer(uint64_t uid, const std::string &stream_name,
                       const std::string& answer,const std::string& stream_type,
                       uint32_t log_id);
        PushStream *find_push_stream(const std::string &string_name);
        void on_connection_state(RtcStream *stream, PeerConnectionState state) override;

        void remove_push_stream(RtcStream *pStream);
    private:
        EventLoop *_el;
        std::unordered_map<std::string, PushStream *> _push_streams;
        std::unique_ptr<PortAllocator>  _allocator;

    };
}

#endif //XRTCSERVER_RTC_STREAM_MANAGER_H
