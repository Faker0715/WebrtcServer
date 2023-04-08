/***************************************************************************
 * 
 * Copyright (c) 2022 str2num.com, Inc. All Rights Reserved
 * $Id$ 
 * 
 **************************************************************************/
 
 
 
/**
 * @file log.h
 * @author str2num
 * @version $Revision$ 
 * @brief 
 *  
 **/



#ifndef  __BASE_LOG_H_
#define  __BASE_LOG_H_

#include <fstream>
#include <queue>
#include <mutex>
#include <thread>

#include <rtc_base/logging.h>

namespace xrtc {

class XrtcLog : public rtc::LogSink {
public:
    XrtcLog(const std::string& log_dir,
            const std::string& log_name,
            const std::string& log_level);
    ~XrtcLog() override;
    
    int init();
    void set_log_to_stderr(bool on);
    bool start();
    void stop();
    void join();

    void OnLogMessage(const std::string& message, rtc::LoggingSeverity severity) override;
    void OnLogMessage(const std::string& message) override;

private:
    std::string _log_dir;
    std::string _log_name;
    std::string _log_level;
    std::string _log_file;
    std::string _log_file_wf;

    std::ofstream _out_file;
    std::ofstream _out_file_wf;

    std::queue<std::string> _log_queue;
    std::mutex _mtx;

    std::queue<std::string> _log_queue_wf;
    std::mutex _mtx_wf;

    std::thread* _thread = nullptr;
    std::atomic<bool> _running{false};
};

} // namespace xrtc


#endif  //__BASE_LOG_H_


