//
// Created by faker on 23-4-19.
//

#include "ice_connection.h"
namespace xrtc{
    IceConnection::IceConnection(EventLoop *el, UDPPort *port, const Candidate &remote_candidate):_el(el),_port(port),_remote_candidate(remote_candidate) {


    }

    IceConnection::~IceConnection() {

    }
}