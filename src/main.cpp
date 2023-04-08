/***************************************************************************
 * 
 * Copyright (c) 2022 str2num.com, Inc. All Rights Reserved
 * $Id$ 
 * 
 **************************************************************************/
 
 
 
/**
 * @file main.cpp
 * @author str2num
 * @version $Revision$ 
 * @brief 
 *  
 **/

#include <signal.h>

#include "base/conf.h"
#include "base/log.h"
#include "server/signaling_server.h"
#include "server/rtc_server.h"

xrtc::GeneralConf* g_conf = nullptr;
xrtc::XrtcLog* g_log = nullptr;
xrtc::SignalingServer* g_signaling_server = nullptr;
xrtc::RtcServer* g_rtc_server = nullptr;

int init_general_conf(const char* filename) {
    if (!filename) {
        fprintf(stderr, "filename is nullptr\n");
        return -1;
    }

    g_conf = new xrtc::GeneralConf();

    int ret = xrtc::load_general_conf(filename, g_conf);
    if (ret != 0) {
        fprintf(stderr, "load %s config file failed\n", filename);
        return -1;
    }

    return 0;
}

int init_log(const std::string& log_dir, const std::string& log_name,
        const std::string& log_level)
{
    g_log = new xrtc::XrtcLog(log_dir, log_name, log_level);

    int ret = g_log->init();
    if (ret != 0) {
        fprintf(stderr, "init log failed\n");
        return -1;
    }
    
    g_log->start();

    return 0;
}

int init_signaling_server() {
    g_signaling_server = new xrtc::SignalingServer();
    int ret = g_signaling_server->init("./conf/signaling_server.yaml");
    if (ret != 0) {
        return -1;
    }

    return 0;
}

int init_rtc_server() {
    g_rtc_server = new xrtc::RtcServer();
    int ret = g_rtc_server->init("./conf/rtc_server.yaml");
    if (ret != 0) {
        return -1;
    }

    return 0;
}

static void process_signal(int sig) {
    RTC_LOG(LS_INFO) << "receive signal: " << sig;
    if (SIGINT == sig || SIGTERM == sig) {
        if (g_signaling_server) {
            g_signaling_server->stop();
        }

        if (g_rtc_server) {
            g_rtc_server->stop();
        }
    }
}
int main() {
    int ret = init_general_conf("./conf/general.yaml");
    if (ret != 0) {
        return -1;
    }
    
    ret = init_log(g_conf->log_dir, g_conf->log_name, g_conf->log_level);
    if (ret != 0) {
        return -1;
    }
    g_log->set_log_to_stderr(g_conf->log_to_stderr);
    // 初始化signaling server
    ret = init_signaling_server();
    if (ret != 0) {
        return -1;
    }
    
    // 初始化rtc server
    ret = init_rtc_server();
    if (ret != 0) {
        return -1;
    }

    signal(SIGINT, process_signal);
    signal(SIGTERM, process_signal);

    g_signaling_server->start();
    g_rtc_server->start();
    
    g_signaling_server->join();
    g_rtc_server->join();

    return 0;
}












