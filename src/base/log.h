#ifndef  __XRTCSERVER_BASE_LOG_H_
#define  __XRTCSERVER_BASE_LOG_H_

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
    
    int Init();
    void SetLogToStderr(bool on);
    bool Start();
    void Stop();
    void Join();

    void OnLogMessage(const std::string& message, rtc::LoggingSeverity severity) override;
    void OnLogMessage(const std::string& message) override;

private:
    std::string log_dir_;
    std::string log_name_;
    std::string log_level_;
    std::string log_file_;
    std::string log_file_wf_;

    std::ofstream out_file_;
    std::ofstream out_file_wf_;

    std::queue<std::string> log_queue_;
    std::mutex mtx_;

    std::queue<std::string> log_queue_wf_;
    std::mutex mtx_wf_;

    std::thread* thread_ = nullptr;
    std::atomic<bool> running_{false};
};

} // namespace xrtc


#endif  //__XRTCSERVER_BASE_LOG_H_


