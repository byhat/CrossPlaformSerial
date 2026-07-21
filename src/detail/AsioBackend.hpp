#ifndef CPS_DETAIL_ASIO_BACKEND_HPP
#define CPS_DETAIL_ASIO_BACKEND_HPP

#include "SerialBackend.hpp"
#include "ReadBuffer.hpp"

#include <cps/SerialTypes.hpp>

#include <boost/asio.hpp>

#include <array>
#include <atomic>
#include <memory>
#include <mutex>
#include <thread>
#include <vector>
#include <string>

#if defined(_WIN32)
#  define CPS_BACKEND_WIN32
#else
#  define CPS_BACKEND_POSIX
#endif

namespace cps {
namespace detail {

/** Desktop backend backed by boost::asio (Linux/Windows). */
class AsioBackend : public SerialBackend {
public:
    AsioBackend();
    ~AsioBackend() override;

    AsioBackend(const AsioBackend&) = delete;
    AsioBackend& operator=(const AsioBackend&) = delete;

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

private:
    void applyConfig();
    void applyNativeTimeouts();
    void startRead();
    void doRead();
    void setError(SerialError e);
    void notifyError(SerialError e);

    // configuration (stored, applied on open + setters)
    std::int32_t baud_ = 9600;
    DataBits     dataBits_ = DataBits::Data8;
    Parity       parity_ = Parity::NoParity;
    StopBits     stopBits_ = StopBits::OneStop;
    FlowControl  flow_ = FlowControl::NoFlowControl;

    std::atomic<SerialError> error_{SerialError::NoError};
    std::atomic<std::int64_t> bytesToWrite_{0};

    std::unique_ptr<boost::asio::io_context> io_;
    std::unique_ptr<boost::asio::serial_port> port_;
    struct WorkGuard;
    std::unique_ptr<WorkGuard> work_;
    std::thread thread_;
    std::atomic<bool> running_{false};
    std::atomic<bool> closing_{false}; // true while we are tearing the port down

    mutable std::mutex bufMx_;
    ReadBuffer readBuffer_;
    std::array<unsigned char, 4096> readScratch_{};
};

} // namespace detail
} // namespace cps

#endif // CPS_DETAIL_ASIO_BACKEND_HPP
