#include "video/rtp_video_stream_receiver.h"

#include <rtc_base/logging.h>

namespace xrtc {
    namespace {

        const int kPacketBufferStartSize = 512;
        const int kPacketBufferMaxSize = 2048;

        std::unique_ptr<RtpRtcpImpl> CreateRtpRtcpModule(
                const VideoReceiveStreamConfig& vconf,
                ReceiveStat* receive_stat)
        {
            RtpRtcpConfig config;
            config.el = vconf.el;
            config.clock = vconf.clock;
            config.local_media_ssrc = vconf.rtp.local_ssrc;
            config.receive_stat = receive_stat;
            config.rtp_rtcp_module_observer = vconf.rtp_rtcp_module_observer;

            auto rtp_rtcp = std::make_unique<RtpRtcpImpl>(config);
            rtp_rtcp->SetRTCPStatus(webrtc::RtcpMode::kCompound);
            return rtp_rtcp;
        }

    } // namespace

    RtpVideoStreamReceiver::RtpVideoStreamReceiver(const VideoReceiveStreamConfig& config,
                                                   ReceiveStat* rtp_receive_stat) :
            config_(config),
            rtp_receive_stat_(rtp_receive_stat),
            rtp_rtcp_(CreateRtpRtcpModule(config, rtp_receive_stat)),
            video_rtp_depacketizer_(std::make_unique<webrtc::VideoRtpDepacketizerH264>()),
            packet_buffer_(std::make_unique<webrtc::video_coding::PacketBuffer>(
                    kPacketBufferStartSize, kPacketBufferMaxSize))
    {
        rtp_rtcp_->SetRemoteSsrc(config.rtp.remote_ssrc);
    }

    RtpVideoStreamReceiver::~RtpVideoStreamReceiver() {

    }

    void RtpVideoStreamReceiver::OnRtpPacket(const webrtc::RtpPacketReceived& packet) {
        ReceivePacket(packet);

        if (!packet.recovered())  {
            rtp_receive_stat_->OnRtpPacket(packet);
        }
    }

    void RtpVideoStreamReceiver::ReceivePacket(const webrtc::RtpPacketReceived& packet) {
        if (0 == packet.payload_size()) {
            return;
        }

        absl::optional<webrtc::VideoRtpDepacketizer::ParsedRtpPayload> parsed_payload =
                video_rtp_depacketizer_->Parse(packet.PayloadBuffer());
        if (absl::nullopt == parsed_payload) {
            RTC_LOG(LS_WARNING) << "parsing rtp payload failed";
            return;
        }

        OnReceivedPayloadData(std::move(parsed_payload->video_payload),
                              packet, parsed_payload->video_header);
    }

    void RtpVideoStreamReceiver::OnReceivedPayloadData(
            rtc::CopyOnWriteBuffer codec_payload,
            const webrtc::RtpPacketReceived& rtp_packet,
            const webrtc::RTPVideoHeader& video)
    {
        auto packet = std::make_unique<webrtc::video_coding::PacketBuffer::Packet>(
                rtp_packet, video);
        webrtc::RTPVideoHeader& video_header = packet->video_header;
        video_header.is_last_packet_in_frame |= rtp_packet.Marker();

        OnInsertedPacket(packet_buffer_->InsertPacket(std::move(packet)));
    }

    void RtpVideoStreamReceiver::OnInsertedPacket(
            webrtc::video_coding::PacketBuffer::InsertResult result)
    {
        if (result.packets.size() <= 0) {
            return;
        }

        webrtc::video_coding::PacketBuffer::Packet* first_packet = nullptr;

        for (auto& packet : result.packets) {
            if (packet->is_first_packet_in_frame()) {
                first_packet = packet.get();
            }

            if (packet->is_last_packet_in_frame()) {
                webrtc::video_coding::PacketBuffer::Packet* last_packet = packet.get();

                OnAssembledFrame(std::make_unique<RtpFrameObject>(
                        first_packet->seq_num,
                        last_packet->seq_num,
                        first_packet->codec(),
                        first_packet->video_header));
            }
        }
    }

    void RtpVideoStreamReceiver::OnAssembledFrame(std::unique_ptr<RtpFrameObject> frame) {
        if (config_.rtp_rtcp_module_observer) {
            config_.rtp_rtcp_module_observer->OnFrame(std::move(frame));
        }
    }

    void RtpVideoStreamReceiver::DeliverRtcp(const uint8_t* data, size_t len) {
        rtp_rtcp_->IncomingRtcpPacket(data, len);
    }

} // namespace xrtc


