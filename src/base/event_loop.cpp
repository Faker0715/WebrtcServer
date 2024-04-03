#include "base/event_loop.h"

#include <libev/ev.h>

#define TRANS_TO_EV_MASK(mask) \
    (((mask) & EventLoop::READ ? EV_READ : 0) | ((mask) & EventLoop::WRITE ? EV_WRITE : 0))

#define TRANS_FROM_EV_MASK(mask) \
    (((mask) & EV_READ ? EventLoop::READ : 0) | ((mask) & EV_WRITE ? EventLoop::WRITE : 0))

namespace xrtc {

EventLoop::EventLoop(void* owner) :
    owner_(owner),
    loop_(ev_loop_new(EVFLAG_AUTO))
{

}

EventLoop::~EventLoop() {

}

void EventLoop::Start() {
    ev_run(loop_);
}

void EventLoop::Stop() {
    ev_break(loop_, EVBREAK_ALL);
}

unsigned long EventLoop::now() {
    return static_cast<unsigned long>(ev_now(loop_) * 1000000);
}

class IOWatcher {
public:
    IOWatcher(EventLoop* el, io_cb_t cb, void* data) :
        el(el), cb(cb), data(data) 
    {
        io.data = this;
    }

public:
    EventLoop* el;
    ev_io io;
    io_cb_t cb;
    void* data;
};

static void GenericIOCb(struct ev_loop* /*loop*/, struct ev_io* io, int events) {
    IOWatcher* watcher = (IOWatcher*)(io->data);
    watcher->cb(watcher->el, watcher, io->fd, TRANS_FROM_EV_MASK(events), 
            watcher->data);
}

IOWatcher* EventLoop::CreateIOEvent(io_cb_t cb, void* data) {
    IOWatcher* w = new IOWatcher(this, cb, data);
    ev_init(&(w->io), GenericIOCb);
    return w;
}

void EventLoop::StartIOEvent(IOWatcher* w, int fd, int mask) {
    struct ev_io* io = &(w->io);
    if (ev_is_active(io)) {
        int active_events = TRANS_FROM_EV_MASK(io->events);
        int events = active_events | mask;
        if (events == active_events) {
            return;
        }

        events = TRANS_TO_EV_MASK(events);
        ev_io_stop(loop_, io);
        ev_io_set(io, fd, events);
        ev_io_start(loop_, io);
    } else {
        int events = TRANS_TO_EV_MASK(mask);
        ev_io_set(io, fd, events);
        ev_io_start(loop_, io);
    }
}

void EventLoop::StopIOEvent(IOWatcher* w, int fd, int mask) {
    struct ev_io* io = &(w->io);
    int active_events = TRANS_FROM_EV_MASK(io->events);
    int events = active_events & ~mask;

    if (events == active_events) {
        return;
    }

    events = TRANS_TO_EV_MASK(events);
    ev_io_stop(loop_, io);
    
    if (events != EV_NONE) {
        ev_io_set(io, fd, events);
        ev_io_start(loop_, io);
    }
}

void EventLoop::DeleteIOEvent(IOWatcher* w) {
    struct ev_io* io = &(w->io);
    ev_io_stop(loop_, io);
    delete w;
}

class TimerWatcher {
public:
    TimerWatcher(EventLoop* el, time_cb_t cb, void* data, bool need_repeat) :
        el(el), cb(cb), data(data), need_repeat(need_repeat)
    {
        timer.data = this;
    }

public:
    EventLoop* el;
    struct ev_timer timer;
    time_cb_t cb;
    void* data;
    bool need_repeat;
};

static void GenericTimeCb(struct ev_loop* /*loop*/, struct ev_timer* timer, int /*events*/) {
    TimerWatcher* watcher = (TimerWatcher*)(timer->data);
    watcher->cb(watcher->el, watcher, watcher->data);
}

TimerWatcher* EventLoop::CreateTimer(time_cb_t cb, void* data, bool need_repeat) {
    TimerWatcher* watcher = new TimerWatcher(this, cb, data, need_repeat);
    ev_init(&(watcher->timer), GenericTimeCb);
    return watcher;
}

void EventLoop::StartTimer(TimerWatcher* w, unsigned int usec) {
    struct ev_timer* timer = &(w->timer);
    float sec = float(usec) / 1000000;

    if (!w->need_repeat) {
        ev_timer_stop(loop_, timer);
        ev_timer_set(timer, sec, 0);
        ev_timer_start(loop_, timer);
    } else {
        timer->repeat = sec;
        ev_timer_again(loop_, timer);
    }
}

void EventLoop::StopTimer(TimerWatcher* w) {
    struct ev_timer* timer = &(w->timer);
    ev_timer_stop(loop_, timer);
}

void EventLoop::DeleteTimer(TimerWatcher* w) {
    StopTimer(w);
    delete w;
}


} // namespace xrtc


