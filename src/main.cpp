#include <iostream>

#include <signal.h>

#include "base/conf.h"
#include "base/log.h"
#include "server/signaling_server.h"
#include "server/rtc_server.h"

xrtc::GeneralConf* g_conf = nullptr;
xrtc::XrtcLog* g_log = nullptr;
xrtc::SignalingServer* g_signaling_server = nullptr;
xrtc::RtcServer* g_rtc_server = nullptr;

int InitGeneralConf(const char* filename) {
    if (!filename) {
        fprintf(stderr, "filename is nullptr\n");
        return -1;
    }

    g_conf = new xrtc::GeneralConf();

    int ret = xrtc::LoadGeneralConf(filename, g_conf);
    if (ret != 0) {
        fprintf(stderr, "load %s config file failed\n", filename);
        return -1;
    }

    return 0;
}

int InitLog(const std::string& log_dir, const std::string& log_name,
        const std::string& log_level)
{
    g_log = new xrtc::XrtcLog(log_dir, log_name, log_level);

    int ret = g_log->Init();
    if (ret != 0) {
        fprintf(stderr, "init log failed\n");
        return -1;
    }
    
    g_log->Start();

    return 0;
}

int InitSignalingServer() {
    g_signaling_server = new xrtc::SignalingServer();
    int ret = g_signaling_server->Init("./conf/signaling_server.yaml");
    if (ret != 0) {
        return -1;
    }

    return 0;
}

int InitRtcServer() {
    g_rtc_server = new xrtc::RtcServer();
    int ret = g_rtc_server->Init("./conf/rtc_server.yaml");
    if (ret != 0) {
        return -1;
    }

    return 0;
}

static void ProcessSignal(int sig) {
    RTC_LOG(LS_INFO) << "receive signal: " << sig;
    if (SIGINT == sig || SIGTERM == sig) {
        if (g_signaling_server) {
            g_signaling_server->Stop();
        }

        if (g_rtc_server) {
            g_rtc_server->Stop();
        }
    }
}


int main() {
    int ret = InitGeneralConf("./conf/general.yaml");
    if (ret != 0) {
        return -1;
    }
    
    ret = InitLog(g_conf->log_dir, g_conf->log_name, g_conf->log_level);
    if (ret != 0) {
        return -1;
    }
    g_log->SetLogToStderr(g_conf->log_to_stderr);
    
    // 初始化signaling server
    ret = InitSignalingServer();
    if (ret != 0) {
        return -1;
    }
    
    // 初始化rtc server
    ret = InitRtcServer();
    if (ret != 0) {
        return -1;
    }

    signal(SIGINT, ProcessSignal);
    signal(SIGTERM, ProcessSignal);

    g_signaling_server->Start();
    g_rtc_server->Start();
    
    g_signaling_server->Join();
    g_rtc_server->Join();

    return 0;
}












