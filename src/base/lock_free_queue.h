/***************************************************************************
 * 
 * Copyright (c) 2022 str2num.com, Inc. All Rights Reserved
 * $Id$ 
 * 
 **************************************************************************/
 
 
 
/**
 * @file lock_free_queue.h
 * @author str2num
 * @version $Revision$ 
 * @brief 
 *  
 **/



#ifndef  __LOCK_FREE_QUEUE_H_
#define  __LOCK_FREE_QUEUE_H_

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
    std::atomic<int> _size;

public:
    LockFreeQueue() {
        first = divider = last = new Node(T());
        _size = 0;
    }

    ~LockFreeQueue() {
        while (first != nullptr) {
            Node* temp = first;
            first = first->next;
            delete temp;
        }

        _size = 0;
    }

    void produce(const T& t) {
        last->next = new Node(t);
        last = last->next;
        ++_size;

        while (divider != first) {
            Node* temp = first;
            first = first->next;
            delete temp;
        }
    }

    bool consume(T* result) {
        if (divider != last) {
            *result = divider->next->value;
            divider = divider->next;
            --_size;
            return true;
        }

        return false;
    }

    bool empty() {
        return _size == 0;
    }

    int size() {
        return _size;
    }
};

} // namespace xrtc

#endif  //__LOCK_FREE_QUEUE_H_


