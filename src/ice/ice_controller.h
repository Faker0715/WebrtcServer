//
// Created by faker on 23-4-21.
//

#ifndef XRTCSERVER_ICE_CONTROLLER_H
#define XRTCSERVER_ICE_CONTROLLER_H

#include "ice_transport_channel.h"

namespace xrtc{
    class IceTransportChannel;

    struct PingResult{
        PingResult(const IceConnection* conn,int ping_intercal):
                conn(conn),ping_interval(ping_intercal){}

        const IceConnection* conn = nullptr;
        int ping_interval = 0;
    };

    class IceController {
    public:
        IceController(IceTransportChannel* ice_channel): _ice_channel(ice_channel) {}
        ~IceController() = default;
        const std::vector<IceConnection*> connections() const { return _connections; }
        void add_connection(IceConnection* conn);
        bool has_pingable_connection();
        PingResult select_connection_to_ping(int64_t last_ping_sent_ms);
    private:
        bool _weak(){
            return _selected_connection == nullptr || _selected_connection->weak();
        }
        bool _is_pingable(IceConnection *conn);
        const IceConnection* _find_next_pingable_connection(int64_t last_ping_sent_ms);
        bool _is_connection_past_ping_interval(const IceConnection* conn, int64_t now);
        int _get_connection_ping_interval(const IceConnection* conn, int64_t now);


    private:
        IceTransportChannel* _ice_channel;
        IceConnection* _selected_connection = nullptr;
        std::vector<IceConnection*> _connections;

    };
}



#endif //XRTCSERVER_ICE_CONTROLLER_H
