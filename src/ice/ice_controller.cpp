//
// Created by faker on 23-4-21.
//

#include "ice_controller.h"
#include "rtc_base/logging.h"
#include "rtc_base/time_utils.h"
#include "absl/algorithm/container.h"

namespace xrtc {
    const int a_is_better = 1;
    const int b_is_better = -1;
    const int k_min_improvement = 10;

    bool IceController::has_pingable_connection() {
        int64_t now = rtc::TimeMillis();
        for (auto conn: _connections) {
            if (_is_pingable(conn,now)) {
                return true;
            }
        }
        return false;
    }

    bool IceController::_is_pingable(IceConnection *conn,int64_t now) {
        const Candidate &remote = conn->remote_candidate();
        if (remote.username.empty() || remote.password.empty()) {
            RTC_LOG(LS_WARNING) << "remote ICE ufrag and pwd is empty,cannot ping.";
            return false;
        }
        if (_weak()) {
            return true;
        }
        return _is_connection_past_ping_interval(conn,now);
    }

    void IceController::add_connection(IceConnection *conn) {
        _connections.push_back(conn);
        _unpinged_connections.insert(conn);
    }

    PingResult IceController::select_connection_to_ping(int64_t last_ping_sent_ms) {
        bool need_ping_more_at_weak = false;

        RTC_LOG(LS_INFO) << "_connection size " << _connections.size();
        for (auto conn: _connections) {
//            RTC_LOG(LS_INFO) << conn->to_string() << " num_pings_sent " << conn->num_pings_sent();
            if (conn->num_pings_sent() < MIN_PINGS_AT_WEAK_PING_INTERVAL) {
                need_ping_more_at_weak = true;
                break;
            }
        }
        int ping_interval = (_weak() || need_ping_more_at_weak) ? WEAK_PING_INTERVAL : STRONG_PING_INTERVAL;
//        if(!need_ping_more_at_weak){
//            RTC_LOG(LS_INFO) << "come in";
//        }
        int64_t now = rtc::TimeMillis();
        const IceConnection *conn = nullptr;
        if (now >= last_ping_sent_ms + ping_interval) {
            conn = _find_next_pingable_connection(now);
        }
        return PingResult(conn, ping_interval);
    }

    const IceConnection *IceController::_find_next_pingable_connection(int64_t now) {
        if (_selected_connection && _selected_connection->writable() &&
            _is_connection_past_ping_interval(_selected_connection, now)) {
            return _selected_connection;
        }

        bool has_pingable = false;
        for (auto conn: _unpinged_connections) {
            if (_is_pingable(conn,now)) {
                has_pingable = true;
                break;
            }
        }

        if (!has_pingable) {
            // 表示一轮结束 继续开启下一轮
            _unpinged_connections.insert(_pinged_connections.begin(),
                                         _pinged_connections.end());
            _pinged_connections.clear();

        }
        IceConnection *find_conn = nullptr;
        for (auto conn: _unpinged_connections) {
            if(!_is_pingable(conn,now)){
                continue;
            }
            // 找出最久没有发送ping请求的conn
            if (_more_pingable(conn, find_conn)) {
                find_conn = conn;
            }
        }
        return find_conn;

    }

    bool IceController::_more_pingable(IceConnection *conn1, IceConnection *conn2) {
        if (!conn2)
            return true;
        if (!conn1)
            return false;
        if (conn1->last_ping_sent() < conn2->last_ping_sent()) {
            return true;
        } else if (conn1->last_ping_sent() > conn2->last_ping_sent()) {
            return false;
        }
        return false;
    }

    bool IceController::_is_connection_past_ping_interval(const IceConnection *conn,
                                                          int64_t now) {
        int interval = _get_connection_ping_interval(conn, now);
        RTC_LOG(LS_INFO) << "========conn ping_interval: " << interval << ", last_ping_sent: " << conn->last_ping_sent();
        return now >= conn->last_ping_sent() + interval;
    }

    int IceController::_get_connection_ping_interval(const IceConnection *conn,
                                                     int64_t now) {
        if (conn->num_pings_sent() < MIN_PINGS_AT_WEAK_PING_INTERVAL) {
            return WEAK_PING_INTERVAL; // 48ms
        }

        if (_weak() || !conn->stable(now)) {
            return STABLING_CONNECTION_PING_INTERVAL; // 900ms
        }

        return STABLE_CONNECTION_PING_INTERVAL; // 2500ms
    }


    int IceController::_compare_connections(IceConnection *a, IceConnection *b) {
        if (a->writable() && !b->writable()) {
            return a_is_better;
        }
        if (!a->writable() && b->writable()) {
            return b_is_better;
        }
        if (a->write_state() < b->write_state()) {
            return a_is_better;
        }
        if (a->write_state() > b->write_state()) {
            return b_is_better;
        }
        if (a->receiving() && !b->receiving()) {
            return a_is_better;
        }
        if (!a->receiving() && b->receiving()) {
            return b_is_better;
        }

        if (a->priority() > b->priority()) {
            return a_is_better;
        }
        if (a->priority() < b->priority()) {
            return b_is_better;
        }


        return 0;
    }

    bool IceController::ready_to_send(IceConnection *conn) {
        return conn && (conn->writable() || conn->write_state() == IceConnection::STATE_WRITE_UNRELIABLE);
    }

    IceConnection *IceController::sort_and_switch_connection() {
        absl::c_stable_sort(_connections, [this](IceConnection *conn1, IceConnection *conn2) {
            int cmp = _compare_connections(conn1, conn2);
            if (cmp != 0) {
                return cmp > 0;
            }
            return conn1->rtt() < conn2->rtt();
        });

        RTC_LOG(LS_INFO) << "Sort " << _connections.size() << " available connections:";
        for (auto conn: _connections) {
            RTC_LOG(LS_INFO) << conn->to_string();
        }
        IceConnection *top_connection = _connections.empty() ? nullptr : _connections[0];
        // 不具备发送条件 以及 排序完了还是自身
        if (!ready_to_send(top_connection) || _selected_connection == top_connection) {
            return nullptr;
        }

        if (!_selected_connection) {
            return top_connection;
        }
        // top_connection rtt 达到一定的值之后才会允许切换
        if (top_connection->rtt() <= _selected_connection->rtt() - k_min_improvement) {
            return top_connection;
        }
    }

    void IceController::mark_connection_pinged(IceConnection *conn) {
        if (conn && _pinged_connections.insert(conn).second) {
            _unpinged_connections.erase(conn);
        }

    }

    void IceController::on_connection_destroy(IceConnection *conn) {
        _pinged_connections.erase(conn);
        _unpinged_connections.erase(conn);
        auto iter = _connections.begin();
        for (; iter != _connections.end(); ++iter) {
            if (*iter == conn) {
                _connections.erase(iter);
                break;
            }
        }
    }
}



