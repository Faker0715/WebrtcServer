#ifndef  __XRTCSERVER_BASE_EVENT_LOOP_H_
#define  __XRTCSERVER_BASE_EVENT_LOOP_H_

struct ev_loop;

namespace xrtc {

class EventLoop;
class IOWatcher;
class TimerWatcher;

typedef void (*io_cb_t)(EventLoop* el, IOWatcher* w, int fd, int events, void* data);
typedef void (*time_cb_t)(EventLoop* el, TimerWatcher* w, void* data);

class EventLoop {
public:
    enum {
        READ = 0x1,
        WRITE= 0x2
    };
     
    EventLoop(void* owner);
    ~EventLoop();
    
    void Start();
    void Stop();
    void* owner() { return owner_; }
    unsigned long now();

    IOWatcher* CreateIOEvent(io_cb_t cb, void* data);
    void StartIOEvent(IOWatcher* w, int fd, int mask);
    void StopIOEvent(IOWatcher* w, int fd, int mask);
    void DeleteIOEvent(IOWatcher* w);
    
    TimerWatcher* CreateTimer(time_cb_t cb, void* data, bool need_repeat);
    void StartTimer(TimerWatcher* w, unsigned int usec);
    void StopTimer(TimerWatcher* w);
    void DeleteTimer(TimerWatcher* w);

private:
    void* owner_;
    struct ev_loop* loop_;
};

} // namespace xrtc


#endif  //__XRTCSERVER_BASE_EVENT_LOOP_H_


