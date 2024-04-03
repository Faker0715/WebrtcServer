#ifndef  __XRTCSERVER_SERVER_LOCK_FREE_QUEUE_H_
#define  __XRTCSERVER_SERVER_LOCK_FREE_QUEUE_H_

#include <atomic>

namespace xrtc {

// 一个生产者，一个消费者
// 指针的操作是原子的

template <typename T>
class LockFreeQueue {
private:
    struct Node {
        T value;
        Node* next;

        Node(const T& value) : value(value), next(nullptr) {} 
    };

    Node* first;
    Node* divider;
    Node* last;
    std::atomic<int> size_;

public:
    LockFreeQueue() {
        first = divider = last = new Node(T());
        size_ = 0;
    }

    ~LockFreeQueue() {
        while (first != nullptr) {
            Node* temp = first;
            first = first->next;
            delete temp;
        }

        size_ = 0;
    }

    void Produce(const T& t) {
        last->next = new Node(t);
        last = last->next;
        ++size_;

        while (divider != first) {
            Node* temp = first;
            first = first->next;
            delete temp;
        }
    }

    bool Consume(T* result) {
        if (divider != last) {
            *result = divider->next->value;
            divider = divider->next;
            --size_;
            return true;
        }

        return false;
    }

    bool empty() {
        return size_ == 0;
    }

    int size() {
        return size_;
    }
};

} // namespace xrtc

#endif  //__XRTCSERVER_SERVER_LOCK_FREE_QUEUE_H_


