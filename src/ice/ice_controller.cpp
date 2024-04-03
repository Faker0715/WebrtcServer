#include "ice/ice_controller.h"

#include <rtc_base/time_utils.h>
#include <rtc_base/logging.h>
#include <absl/algorithm/container.h>

namespace xrtc {

const int kMinImprovement = 10;
const int a_is_better = 1;
const int b_is_better = -1;

void IceController::AddConnection(IceConnection* conn) {
    connections_.push_back(conn);
    unpinged_connections_.insert(conn);
}

void IceController::OnConnectionDestroyed(IceConnection* conn) {
    pinged_connections_.erase(conn);
    unpinged_connections_.erase(conn);
    
    auto iter = connections_.begin();
    for (; iter != connections_.end(); ++iter) {
        if (*iter == conn) {
            connections_.erase(iter);
            break;
        }
    }
}

bool IceController::HasPingableConnection() {
    int64_t now = rtc::TimeMillis();
    for (auto conn : connections_) {
        if (IsPingable(conn, now)) {
            return true;
        }
    }

    return false;
}

bool IceController::IsPingable(IceConnection* conn, int64_t now) {
    const Candidate& remote = conn->remote_candidate();
    if (remote.username.empty() || remote.password.empty()) {
        RTC_LOG(LS_WARNING) << "remote ICE ufrag and pwd is empty, cannot ping.";
        return false;
    }

    if (weak()) {
        return true;
    }

    return IsConnectionPastPingInterval(conn, now);
}


PingResult IceController::SelectConnectionToPing(int64_t last_ping_sent_ms) {
    bool need_ping_more_at_weak = false;
    for (auto conn : connections_) {
        if (conn->num_pings_sent() < MIN_PINGS_AT_WEAK_PING_INTERVAL) {
            need_ping_more_at_weak = true;
            break;
        }
    }

    int ping_interval = (weak() || need_ping_more_at_weak) ? WEAK_PING_INTERVAL
        : STRONG_PING_INTERVAL;
    
    int64_t now = rtc::TimeMillis();
    const IceConnection* conn = nullptr;
    if (now >= last_ping_sent_ms + ping_interval) {
        conn = FindNextPingableConnection(now);
    }

    return PingResult(conn, ping_interval);
}

const IceConnection* IceController::FindNextPingableConnection(int64_t now) {
    if (selected_connection_ && selected_connection_->writable() &&
            IsConnectionPastPingInterval(selected_connection_, now))
    {
        return selected_connection_;
    }
    
    bool has_pingable = false;
    for (auto conn : unpinged_connections_) {
        if (IsPingable(conn, now)) {
            has_pingable = true;
            break;
        }
    }
    
    if (!has_pingable) {
        unpinged_connections_.insert(pinged_connections_.begin(),
                pinged_connections_.end());
        pinged_connections_.clear();
    }
    
    IceConnection* find_conn = nullptr;
    for (auto conn : unpinged_connections_) {
        if (!IsPingable(conn, now)) {
            continue;
        }

        if (MorePingable(conn, find_conn)) {
            find_conn = conn;
        }
    }

    return find_conn;
}

bool IceController::MorePingable(IceConnection* conn1, IceConnection* conn2) {
    if (!conn2) {
        return true;
    }

    if (!conn1) {
        return false;
    }
    
    if (conn1->last_ping_sent() < conn2->last_ping_sent()) {
        return true;
    }
    
    if (conn1->last_ping_sent() > conn2->last_ping_sent()) {
        return false;
    }

    return false;
}

bool IceController::IsConnectionPastPingInterval(const IceConnection* conn,
        int64_t now)
{
    int interval = GetConnectionPingInterval(conn, now);
    return now >= conn->last_ping_sent() + interval;
}

int IceController::GetConnectionPingInterval(const IceConnection* conn,
        int64_t now)
{
    if (conn->num_pings_sent() < MIN_PINGS_AT_WEAK_PING_INTERVAL) {
        return WEAK_PING_INTERVAL; // 48ms
    }

    if (weak() || !conn->stable(now)) {
        return STABLING_CONNECTION_PING_INTERVAL; // 900ms
    }

    return STABLE_CONNECTION_PING_INTERVAL; // 2500ms
}

int IceController::CompareConnections(IceConnection* a, IceConnection* b) {
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

bool IceController::ReadyToSend(IceConnection* conn) {
    return conn && (conn->writable() || conn->write_state() 
            == IceConnection::STATE_WRITE_UNRELIABLE);
}

IceConnection* IceController::SortAndSwitchConnection() {
    absl::c_stable_sort(connections_, [this](IceConnection* conn1, IceConnection* conn2){
        int cmp = CompareConnections(conn1, conn2);
        if (cmp != 0) {
            return cmp > 0;
        }
        
        return conn1->rtt() < conn2->rtt();
    });
    
    RTC_LOG(LS_INFO) << "Sort " << connections_.size() << " available connetions:";
    for (auto conn : connections_) {
        RTC_LOG(LS_INFO) << conn->ToString();
    }
    
    IceConnection* top_connection = connections_.empty() ? nullptr : connections_[0];
    if (!ReadyToSend(top_connection) || selected_connection_ == top_connection) {
        return nullptr;
    }
    
    if (!selected_connection_) {
        return top_connection;
    }

    if (top_connection->rtt() <= selected_connection_->rtt() - kMinImprovement) {
        return top_connection;
    }

    return nullptr;
}

void IceController::MarkConnectionPinged(IceConnection* conn) {
    if (conn && pinged_connections_.insert(conn).second) {
        unpinged_connections_.erase(conn);
    }
}

} // namespace xrtc


