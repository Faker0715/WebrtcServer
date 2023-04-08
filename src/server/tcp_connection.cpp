/***************************************************************************
 * 
 * Copyright (c) 2022 str2num.com, Inc. All Rights Reserved
 * $Id$ 
 * 
 **************************************************************************/
 
 
 
/**
 * @file tcp_connection.cpp
 * @author str2num
 * @version $Revision$ 
 * @brief 
 *  
 **/

#include "server/tcp_connection.h"
#include "rtc_base/zmalloc.h"

namespace xrtc {

TcpConnection::TcpConnection(int fd) : 
    fd(fd),
    querybuf(sdsempty())
{
}

TcpConnection::~TcpConnection() {
    sdsfree(querybuf);
    while(!reply_list.empty()){
        rtc::Slice reply = reply_list.front();
        zfree((void*)reply.data());
        reply_list.pop_front();
    }
    reply_list.clear();
}


} // namespace xrtc



















