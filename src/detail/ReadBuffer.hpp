#ifndef CPS_DETAIL_READ_BUFFER_HPP
#define CPS_DETAIL_READ_BUFFER_HPP

#include <cstddef>
#include <cstdint>
#include <mutex>
#include <vector>

namespace cps {
namespace detail {

/** Thread-safe byte ring used to buffer data read by the worker thread. */
class ReadBuffer {
public:
    ReadBuffer() = default;

    void push(const std::uint8_t* data, std::size_t size) {
        std::lock_guard<std::mutex> lk(mutex_);
        buf_.insert(buf_.end(), data, data + size);
    }

    /** Pop up to max bytes; returns bytes copied. */
    std::size_t pop(std::uint8_t* out, std::size_t max) {
        std::lock_guard<std::mutex> lk(mutex_);
        std::size_t n = buf_.size() < max ? buf_.size() : max;
        if (n) {
            std::copy(buf_.begin(), buf_.begin() + n, out);
            buf_.erase(buf_.begin(), buf_.begin() + n);
        }
        return n;
    }

    std::size_t size() const {
        std::lock_guard<std::mutex> lk(mutex_);
        return buf_.size();
    }

    void clear() {
        std::lock_guard<std::mutex> lk(mutex_);
        buf_.clear();
    }

private:
    mutable std::mutex mutex_;
    std::vector<std::uint8_t> buf_;
};

} // namespace detail
} // namespace cps

#endif // CPS_DETAIL_READ_BUFFER_HPP
