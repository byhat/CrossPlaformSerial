#ifndef CPS_SERIAL_PORT_HPP
#define CPS_SERIAL_PORT_HPP

#include <cps/cps_export.h>
#include <cps/SerialTypes.hpp>
#include <cps/SerialPortInfo.hpp>

#include <cstdint>
#include <cstddef>
#include <string>
#include <vector>
#include <functional>

namespace cps {

/**
 * @brief Cross-platform serial port.
 *
 * Desktop (Linux/Windows) is backed by boost::asio; Android is backed by JNI into
 * usb-serial-for-android. The platform backend is selected at compile time and hidden
 * behind a fast-pimpl.
 *
 * Threading: a single internal worker thread performs reads and dispatches writes.
 * The user callbacks set via setCallbacks() are invoked FROM THAT WORKER THREAD.
 * Consumers that need to handle data on a different thread (e.g. a Qt GUI thread)
 * must marshal the notification themselves.
 */
class CPS_API SerialPort {
public:
    SerialPort();
    explicit SerialPort(const std::string& portName);
    explicit SerialPort(const SerialPortInfo& info);
    ~SerialPort();

    SerialPort(const SerialPort&)            = delete;
    SerialPort& operator=(const SerialPort&) = delete;
    SerialPort(SerialPort&&) noexcept;
    SerialPort& operator=(SerialPort&&) noexcept;

    void setPortName(const std::string& name);
    std::string portName() const;

    bool open(OpenMode mode = OpenMode::ReadWrite);
    void close();
    bool isOpen() const;

    /** Returns bytes written (queued) or -1 on error. */
    std::int64_t write(const void* data, std::size_t size);
    std::int64_t write(const std::vector<std::uint8_t>& data);
    /** Returns bytes read into buf (0 if none available) or -1 on error. */
    std::int64_t read(void* buf, std::size_t maxSize);
    std::vector<std::uint8_t> readAll();

    bool setBaudRate(std::int32_t rate, Directions d = Directions::AllDirections);
    std::int32_t baudRate(Directions d = Directions::AllDirections) const;
    bool setDataBits(DataBits b);   DataBits dataBits() const;
    bool setParity(Parity p);       Parity parity() const;
    bool setStopBits(StopBits s);   StopBits stopBits() const;
    bool setFlowControl(FlowControl f);  FlowControl flowControl() const;

    bool clear(Directions d = Directions::AllDirections);
    bool flush();
    std::int64_t bytesAvailable() const;
    std::int64_t bytesToWrite() const;
    SerialError error() const;
    void clearError();

    /** Set of optional user callbacks. Replace the whole set at once. */
    struct CPS_API Callbacks {
        std::function<void()>             onReadyRead;     // data available to read
        std::function<void(std::int64_t)> onBytesWritten;  // bytes flushed to device
        std::function<void(SerialError)>  onError;         // an error occurred
    };
    void setCallbacks(Callbacks cbs);

private:
    class Impl;
    // Fast pimpl: Impl is placement-new'd into mStorage (no heap alloc, no indirection).
    static constexpr std::size_t kImplSize  = 1024;  // bump if the static_assert in .cpp fires
    static constexpr std::size_t kImplAlign = alignof(std::max_align_t);
    alignas(kImplAlign) std::byte mStorage[kImplSize];
    Impl&       impl() noexcept;
    const Impl& impl() const noexcept;
};

} // namespace cps

#endif // CPS_SERIAL_PORT_HPP
