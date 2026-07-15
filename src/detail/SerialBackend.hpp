#ifndef CPS_DETAIL_SERIAL_BACKEND_HPP
#define CPS_DETAIL_SERIAL_BACKEND_HPP

#include <cps/SerialTypes.hpp>

#include <cstddef>
#include <cstdint>
#include <functional>
#include <string>

namespace cps {
namespace detail {

/**
 * @brief Abstract platform backend for SerialPort.
 *
 * Threading contract: the backend owns its worker thread. Read/write/error notifications
 * are delivered via the std::function callbacks, invoked FROM THE BACKEND WORKER THREAD.
 * The implementation must guarantee that the public methods below (open/close/read/write/
 * config/bytesAvailable/flush/clear) are safe to call concurrently with the worker thread.
 */
class SerialBackend {
public:
    virtual ~SerialBackend() = default;

    virtual bool open(const std::string& name, OpenMode mode) = 0;
    virtual void close() = 0;
    virtual bool isOpen() const = 0;

    virtual bool setBaudRate(std::int32_t rate) = 0;
    virtual bool setDataBits(DataBits b) = 0;
    virtual bool setParity(Parity p) = 0;
    virtual bool setStopBits(StopBits s) = 0;
    virtual bool setFlowControl(FlowControl f) = 0;

    /** Queue bytes to write; returns the number accepted (queued) or -1 on error. */
    virtual std::int64_t write(const void* data, std::size_t size) = 0;
    /** Pop up to max bytes into buf; returns bytes read (0 if none) or -1 on error. */
    virtual std::int64_t read(void* buf, std::size_t max) = 0;
    virtual std::int64_t bytesAvailable() const = 0;
    virtual std::int64_t bytesToWrite() const = 0;

    virtual bool flush() = 0;
    virtual bool clear(Directions d) = 0;

    virtual SerialError lastError() const = 0;
    virtual void clearError() = 0;

    /// New data is available in the read buffer (read from worker thread).
    std::function<void()>            onReadyRead;
    /// N bytes were flushed to the device (read from worker thread).
    std::function<void(std::int64_t)> onBytesWritten;
    /// An error occurred (read from worker thread).
    std::function<void(SerialError)>  onError;
};

} // namespace detail
} // namespace cps

#endif // CPS_DETAIL_SERIAL_BACKEND_HPP
