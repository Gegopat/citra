// Copyright 2010 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <algorithm>
#include <atomic>
#include <condition_variable>
#include <cstddef>
#include <mutex>
#include <optional>
#include "common/common_types.h"

namespace Common {

// A simple lockless thread-safe single reader, single writer queue
template <typename T, bool NeedSize = true>
class SPSCQueue {
public:
    SPSCQueue() {
        write_ptr = read_ptr = new ElementPtr();
    }

    ~SPSCQueue() {
        // this will empty out the whole queue
        delete read_ptr;
    }

    u32 Size() const {
        static_assert(NeedSize, "using Size() on FifoQueue without NeedSize");
        return size.load();
    }

    bool Empty() const {
        return !read_ptr->next.load();
    }

    T& Front() const {
        return read_ptr->current;
    }

    template <typename Arg>
    void Push(Arg&& t) {
        // Add the element to the queue
        write_ptr->current = std::move(t);
        // Set the next pointer to a new element pointer then advance the write pointer
        ElementPtr* new_ptr{new ElementPtr()};
        write_ptr->next.store(new_ptr, std::memory_order_release);
        write_ptr = new_ptr;
        if (NeedSize)
            size++;
        cv.notify_one();
    }

    void Pop() {
        if (NeedSize)
            size--;
        auto tmpptr{read_ptr};
        // Advance the read pointer
        read_ptr = tmpptr->next.load();
        // Set the next element to nullptr to stop the recursive deletion
        tmpptr->next.store(nullptr);
        delete tmpptr; // this also deletes the element
    }

    bool Pop(T& t) {
        if (Empty())
            return false;
        if (NeedSize)
            size--;
        auto tmpptr{read_ptr};
        read_ptr = tmpptr->next.load(std::memory_order_acquire);
        t = std::move(*tmpptr->current);
        tmpptr->next.store(nullptr);
        delete tmpptr;
        return true;
    }

    T PopWait() {
        if (Empty()) {
            std::unique_lock lock{cv_mutex};
            cv.wait(lock, [this]() { return !Empty(); });
        }
        T t;
        Pop(t);
        return t;
    }

    // Not thread-safe
    void Clear() {
        size.store(0);
        delete read_ptr;
        write_ptr = read_ptr = new ElementPtr();
    }

private:
    // Stores a pointer to element
    // and a pointer to the next ElementPtr
    class ElementPtr {
    public:
        ~ElementPtr() {
            auto next_ptr{next.load()};
            if (next_ptr)
                delete next_ptr;
        }

        std::optional<T> current;
        std::atomic<ElementPtr*> next{};
    };

    ElementPtr* write_ptr;
    ElementPtr* read_ptr;
    std::atomic<u32> size{};
    std::mutex cv_mutex;
    std::condition_variable cv;
};

// A simple thread-safe, single reader, multiple writer queue
template <typename T, bool NeedSize = true>
class MPSCQueue {
public:
    u32 Size() const {
        return spsc_queue.Size();
    }

    bool Empty() const {
        return spsc_queue.Empty();
    }

    T& Front() const {
        return spsc_queue.Front();
    }

    template <typename Arg>
    void Push(Arg&& t) {
        std::lock_guard lock{write_lock};
        spsc_queue.Push(t);
    }

    void Finalize() {
        spsc_queue.Finalize();
    }

    void Pop() {
        return spsc_queue.Pop();
    }

    bool Pop(T& t) {
        return spsc_queue.Pop(t);
    }

    T PopWait() {
        return spsc_queue.PopWait();
    }

    // Not thread-safe
    void Clear() {
        spsc_queue.Clear();
    }

private:
    SPSCQueue<T, NeedSize> spsc_queue;
    std::mutex write_lock;
};

} // namespace Common
