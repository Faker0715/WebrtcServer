#include "base/conf.h"

#include <stdio.h>
#include <iostream>

#include <yaml-cpp/yaml.h>

namespace xrtc {

int LoadGeneralConf(const char* filename, GeneralConf* conf) {
    if (!filename || !conf) {
        fprintf(stderr, "filename or conf is nullptr\n");
        return -1;
    }

    conf->log_dir = "./log";
    conf->log_name = "undefined";
    conf->log_level = "info";
    conf->log_to_stderr = false;
    
    YAML::Node config = YAML::LoadFile(filename);
  
    try {
        conf->log_dir = config["log"]["log_dir"].as<std::string>();
        conf->log_name = config["log"]["log_name"].as<std::string>();
        conf->log_level = config["log"]["log_level"].as<std::string>();
        conf->log_to_stderr = config["log"]["log_to_stderr"].as<bool>();
        conf->ice_min_port = config["ice"]["min_port"].as<int>();
        conf->ice_max_port = config["ice"]["max_port"].as<int>();
        conf->rtcp_report_timer_interval = 
            config["rtp_rtcp"]["rtcp_report_timer_interval"].as<int>();
    } catch (const YAML::Exception& e) {
        fprintf(stderr, "catch a YAML::Exception, line: %d, column: %d"
                ", error:%s\n", e.mark.line + 1, e.mark.column + 1, e.msg.c_str());
        return -1;
    }

    return 0;
}

} // namespace xrtc


















