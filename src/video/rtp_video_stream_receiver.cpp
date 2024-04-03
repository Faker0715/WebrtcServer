//
// Created by faker on 24-1-30.
//

#include "rtp_video_stream_receiver.h"
#include "rtc_base/logging.h"

namespace xrtc {

    namespace {
        std::unique_ptr<RtpRtcpImpl> CreateRtpRtcpModule(const VideoReceiveStreamConfig &vconf,ReceiveStat* receive_stat) {
            RtpRtcpConfig config;
            config.el_ = vconf.el;
            config.clock_ = vconf.clock;
            config.local_media_ssrc = vconf.rtp.local_ssrc;
            config.receive_stat = receive_stat;
            config.rtp_rtcp_module_observer = vconf.rtp_rtcp_module_observer;
            auto rtp_rtcp = std::make_unique<RtpRtcpImpl>(config);
            rtp_rtcp->SetRTCPStatus(webrtc::RtcpMode::kCompound);
            return rtp_rtcp;
        }
    }

    RtpVideoStreamReceiver::~RtpVideoStreamReceiver() {


    }

    void RtpVideoStreamReceiver::OnRtpPacket(const webrtc::RtpPacketReceived &packet) {
        ReceivePacket(packet);
        // 重传包不统计
        if(!packet.recovered()){
            rtp_receive_stat_->OnRtpPacket(packet);
        }

    }
    void RtpVideoStreamReceiver::ReceivePacket(const webrtc::RtpPacketReceived& packet){
        if(0 == packet.payload_size()){
            return;
        }
        absl::optional<webrtc::VideoRtpDepacketizer::ParsedRtpPayload> parsed_payload =
                video_rtp_depacketizer_->Parse(packet.PayloadBuffer());
        if(absl::nullopt == parsed_payload){
            RTC_LOG(LS_WARNING) << "====failed to parse rtp payload";
            return;
        }
        OnReceivedPayloadData(std::move(parsed_payload->video_payload),packet,parsed_payload->video_header);


    }
    void RtpVideoStreamReceiver::OnReceivedPayloadData(
            rtc::CopyOnWriteBuffer codec_payload,
            const webrtc::RtpPacketReceived& packet,
            const webrtc::RTPVideoHeader& video_header) {

    }


    RtpVideoStreamReceiver::RtpVideoStreamReceiver(const VideoReceiveStreamConfig &config,
                                                   ReceiveStat *rtp_receive_stat):
                                                   config_(config),
                                                   rtp_receive_stat_(rtp_receive_stat),
                                                   rtp_rtcp_(CreateRtpRtcpModule(config,rtp_receive_stat)),
                                                   video_rtp_depacketizer_(std::make_unique<webrtc::VideoRtpDepacketizerH264>()) {
        rtp_rtcp_->SetRemoteSSRC(config.rtp.remote_ssrc);
    }

    void RtpVideoStreamReceiver::DeliverRtcp(const uint8_t *data, size_t length) {
        rtp_rtcp_->IncomingRTCPPacket(data, length);
    }
}