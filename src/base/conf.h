/***************************************************************************
 * 
 * Copyright (c) 2022 str2num.com, Inc. All Rights Reserved
 * $Id$ 
 * 
 **************************************************************************/
 
 
 
/**
 * @file conf.h
 * @author str2num
 * @version $Revision$ 
 * @brief 
 *  
 **/



#ifndef  __BASE_CONF_H_
#define  __BASE_CONF_H_

#include <string>

namespace xrtc {

struct GeneralConf {
    std::string log_dir;
    std::string log_name;
    std::string log_level;
    bool log_to_stderr;
    int ice_min_port = 0;
    int ice_max_port = 0;

};

int load_general_conf(const char* filename, GeneralConf* conf);

} // namespace xrtc


#endif  //__BASE_CONF_H_


