
#ifndef  __CODEC_INFO_H_
#define  __CODEC_INFO_H_

#include <string>
#include <vector>
#include <map>

namespace xrtc {

class AudioCodecInfo;
class VideoCodecInfo;

class FeedbackParam {
public:
    FeedbackParam(const std::string& id, const std::string& param) :
        _id(id), _param(param) {}
    FeedbackParam(const std::string& id) : _id(id), _param("") {}

    std::string id() { return _id; }
    std::string param() { return _param; }

private:
    std::string _id;
    std::string _param;
};

typedef std::map<std::string, std::string> CodecParam;

class CodecInfo {
public:
    virtual AudioCodecInfo* as_audio() { return nullptr; }
    virtual VideoCodecInfo* as_video() { return nullptr; }

public:
    int id;
    std::string name;
    int clockrate;
    std::vector<FeedbackParam> feedback_param;
    CodecParam codec_param;
};

class AudioCodecInfo : public CodecInfo {
public:
    AudioCodecInfo* as_audio() override { return this; }

public:
    int channels;
};

class VideoCodecInfo : public CodecInfo {
public:
    VideoCodecInfo* as_video() override { return this; }
};

} // namespace xrtc

#endif  //__CODEC_INFO_H_


