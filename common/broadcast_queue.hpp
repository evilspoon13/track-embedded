#ifndef TRACK_BROADCAST_QUEUE_HPP
#define TRACK_BROADCAST_QUEUE_HPP

#include <atomic>
#include <cstring>

template <typename T, std::size_t Capacity = 4096>
class BroadcastQueue {
    static_assert((Capacity & (Capacity - 1)) == 0, "capacity must be a power of 2");

public:
    BroadcastQueue() = default;

    BroadcastQueue(const BroadcastQueue&) = delete;
    BroadcastQueue& operator=(const BroadcastQueue&) = delete;

    // push a new item — overwrites oldest if full
    void push(const T& item);

    // consume all new items since consumer_pos, calling callback on each
    // consumer_pos is updated to the current write position after consuming
    template <typename Callback>
    void consume(std::size_t& consumer_pos, Callback callback);

    // get the current write index - call once to initialize a new consumer
    std::size_t current_pos() const;

private:
    T buffer_[Capacity];
    std::atomic<std::size_t> write_idx_{0};
};

template <typename T, std::size_t Capacity>
void BroadcastQueue<T, Capacity>::push(const T& item) {
    // only one writer, relaxed is good
    std::size_t idx = write_idx_.load(std::memory_order_relaxed);
    buffer_[idx & (Capacity - 1)] = item;
    write_idx_.store(idx + 1, std::memory_order_release);
}

template <typename T, std::size_t Capacity>
template <typename Callback>
void BroadcastQueue<T, Capacity>::consume(std::size_t& consumer_pos, Callback callback) {
    std::size_t idx = write_idx_.load(std::memory_order_acquire);

    while(consumer_pos < idx) {
        callback(buffer_[consumer_pos++ & (Capacity - 1)]);

    }
}

template <typename T, std::size_t Capacity>
std::size_t BroadcastQueue<T, Capacity>::current_pos() const {
    return write_idx_.load(std::memory_order_acquire);
}


#endif
