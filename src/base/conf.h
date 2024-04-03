#ifndef  __XRTCSERVER_BASE_CONF_H_
#define  __XRTCSERVER_BASE_CONF_H_

#include <string>

namespace xrtc {

struct GeneralConf {
    std::string log_dir;
    std::string log_name;
    std::string log_level;
    bool log_to_stderr;
    int ice_min_port = 0;
    int ice_max_port = 0;
    int rtcp_report_timer_interval = 100;
};

int LoadGeneralConf(const char* filename, GeneralConf* conf);

} // namespace xrtc


#endif  //__XRTCSERVER_BASE_CONF_H_


