#pragma once

#include <algorithm>
#include <atomic>
#include <cstddef>
#include <span>
#include <type_traits>
#include <vector>

/// What the audio threads of miniaudio and the game loop share (docs/architecture/threads.md): a ring
/// buffer without locks.
namespace rtype::voice
{

/// A ring buffer of one producer thread and one consumer thread, without locks: neither ever waits on the
/// other. The producer writes what fits and drops the rest; the consumer reads what is there.
///
/// @code
/// RingBuffer<std::int16_t> samples(SampleRate);    // one second
/// // On the audio thread of the microphone
/// samples.write(captured);
/// // On the game loop
/// std::array<std::int16_t, FrameSamples> frame;
/// if (samples.size() >= frame.size()) {
///     samples.read(frame);
/// }
/// @endcode
///
/// write() is called by the producer only, read() and discard() by the consumer only; size() and capacity()
/// by either.
template <typename T>
    requires std::is_trivially_copyable_v<T>
class RingBuffer
{
    std::vector<T> _items;
    // Counters that only grow, the index being the counter modulo the capacity: full and empty differ.
    // Apart, so that the two threads do not share a cache line
    alignas(64) std::atomic<std::size_t> _read{0};
    alignas(64) std::atomic<std::size_t> _written{0};

public:
    /// @param capacity The most items it holds, at least 1.
    explicit RingBuffer(std::size_t capacity) : _items(std::max<std::size_t>(capacity, 1)) {}

    RingBuffer(const RingBuffer &) = delete;
    RingBuffer &operator=(const RingBuffer &) = delete;

    [[nodiscard]] std::size_t capacity() const { return _items.size(); }

    /// The items there now: exact for either thread about its own side, at least as many for the consumer,
    /// at most as many for the producer.
    [[nodiscard]] std::size_t size() const
    {
        return _written.load(std::memory_order_acquire) - _read.load(std::memory_order_acquire);
    }

    /// Producer: adds what fits of the items, in order.
    /// @return The items added: fewer than given when it is full.
    std::size_t write(std::span<const T> items)
    {
        const std::size_t written = _written.load(std::memory_order_relaxed);
        const std::size_t read = _read.load(std::memory_order_acquire);
        const std::size_t count = std::min(items.size(), capacity() - (written - read));
        copy(items.first(count), written);
        _written.store(written + count, std::memory_order_release);
        return count;
    }

    /// Consumer: takes the oldest items, as many as fit.
    /// @return The items taken: fewer than the room given when it holds fewer.
    std::size_t read(std::span<T> items)
    {
        const std::size_t read = _read.load(std::memory_order_relaxed);
        const std::size_t written = _written.load(std::memory_order_acquire);
        const std::size_t count = std::min(items.size(), written - read);
        for (std::size_t i = 0; i < count; ++i) {
            items[i] = _items[(read + i) % capacity()];
        }
        _read.store(read + count, std::memory_order_release);
        return count;
    }

    /// Consumer: forgets every item there now.
    void discard() { _read.store(_written.load(std::memory_order_acquire), std::memory_order_release); }

private:
    void copy(std::span<const T> items, std::size_t at)
    {
        for (std::size_t i = 0; i < items.size(); ++i) {
            _items[(at + i) % capacity()] = items[i];
        }
    }
};

} // namespace rtype::voice
