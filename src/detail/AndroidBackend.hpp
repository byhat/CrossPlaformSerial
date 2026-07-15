#ifndef CPS_DETAIL_ANDROID_BACKEND_HPP
#define CPS_DETAIL_ANDROID_BACKEND_HPP

#include "SerialBackend.hpp"
#include "ReadBuffer.hpp"
#include "PortEnumerator.hpp"

#include <cps/SerialTypes.hpp>

#include <atomic>
#include <cstdint>
#include <mutex>
#include <string>

namespace cps {
namespace detail {

/**
 * @brief Android backend: drives usb-serial-for-android (loaded from an embedded
 *        classes.dex) via JNI.
 *
 * Only compiled under __ANDROID__. The matching Java bridge lives in
 * android/java/com/cps/android/CpsUsbSerial.java.
 */
class AndroidBackend : public SerialBackend {
public:
    AndroidBackend();
    ~AndroidBackend() override;

    AndroidBackend(const AndroidBackend&) = delete;
    AndroidBackend& operator=(const AndroidBackend&) = delete;

    bool open(const std::string& name, OpenMode mode) override;
    void close() override;
    bool isOpen() const override;

    bool setBaudRate(std::int32_t rate) override;
    bool setDataBits(DataBits b) override;
    bool setParity(Parity p) override;
    bool setStopBits(StopBits s) override;
    bool setFlowControl(FlowControl f) override;

    std::int64_t write(const void* data, std::size_t size) override;
    std::int64_t read(void* buf, std::size_t max) override;
    std::int64_t bytesAvailable() const override;
    std::int64_t bytesToWrite() const override;
    bool flush() override;
    bool clear(Directions d) override;

    SerialError lastError() const override;
    void clearError() override;

    /** Called from the Java I/O-manager thread when new bytes arrive. */
    void handleData(const std::uint8_t* data, std::size_t n);

private:
    void registerHandle();
    void unregisterHandle();

    std::int32_t baud_ = 9600;
    DataBits    dataBits_ = DataBits::Data8;
    Parity      parity_   = Parity::NoParity;
    StopBits    stopBits_ = StopBits::OneStop;
    FlowControl flow_     = FlowControl::NoFlowControl;

    std::atomic<bool> open_{false};
    std::atomic<SerialError> error_{SerialError::NoError};

    mutable std::mutex bufMx_;
    ReadBuffer readBuffer_;
};

/** One-time Android initialization (loads the embedded DEX, caches JNI ids). */
SerialError androidInit(void* java_vm, void* android_context);

} // namespace detail
} // namespace cps

#endif // CPS_DETAIL_ANDROID_BACKEND_HPP
