#include "AsioBackend.hpp"

#include <boost/asio.hpp>
#include <boost/system/error_code.hpp>

#include <algorithm>
#include <cerrno>
#include <cstring>

#if defined(CPS_BACKEND_POSIX)
#  include <fcntl.h>
#  include <termios.h>
#  include <unistd.h>
#  include <dirent.h>
#  include <cstdio>
#  include <fstream>
#else
#  ifndef WIN32_LEAN_AND_MEAN
#    define WIN32_LEAN_AND_MEAN
#  endif
#  ifndef NOMINMAX
#    define NOMINMAX
#  endif
#  include <windows.h>
#endif

namespace cps {
namespace detail {

namespace asio = boost::asio;
using boost::system::error_code;

// ---- error mapping --------------------------------------------------------

static SerialError mapError(const error_code& ec) {
    if (!ec) return SerialError::NoError;
    if (ec == asio::error::operation_aborted) return SerialError::NoError; // closing
    int v = ec.value();
#if defined(CPS_BACKEND_POSIX)
    if (v == ENOENT || v == ENXIO)         return SerialError::DeviceNotFoundError;
    if (v == EACCES || v == EPERM)         return SerialError::PermissionError;
    if (v == EBUSY)                        return SerialError::ResourceError;
    if (v == EIO || v == EPIPE || v == ENODEV)
                                            return SerialError::ResourceError;
#else
    if (v == 2L)   return SerialError::DeviceNotFoundError;   // ERROR_FILE_NOT_FOUND
    if (v == 5L)   return SerialError::PermissionError;       // ERROR_ACCESS_DENIED
    if (v == 32L)  return SerialError::ResourceError;         // ERROR_SHARING_VIOLATION
#endif
    return SerialError::UnknownError;
}

// ---- work guard type ------------------------------------------------------

struct AsioBackend::WorkGuard {
    asio::executor_work_guard<asio::io_context::executor_type> guard;
    explicit WorkGuard(asio::executor_work_guard<asio::io_context::executor_type> g)
        : guard(std::move(g)) {}
};

// ---- lifecycle ------------------------------------------------------------

AsioBackend::AsioBackend() = default;

AsioBackend::~AsioBackend() {
    close();
}

bool AsioBackend::open(const std::string& name, OpenMode mode) {
    if (!io_)  io_   = std::make_unique<asio::io_context>();
    if (!port_) port_ = std::make_unique<asio::serial_port>(*io_);

    if (port_->is_open()) close();
    (void)mode; // POSIX/Windows devices are opened read/write by asio.

    error_code ec;
    port_->open(name, ec);
    if (ec) { setError(mapError(ec)); return false; }

    error_.store(SerialError::NoError);
    applyConfig();
    applyNativeTimeouts();

    running_ = true;
    work_ = std::make_unique<WorkGuard>(asio::make_work_guard(*io_));
    if (!thread_.joinable())
        thread_ = std::thread([this]() { io_->run(); });

    startRead();
    return true;
}

void AsioBackend::close() {
    running_ = false;

    if (port_ && port_->is_open()) {
        error_code ec;
        port_->cancel(ec);
    }
    work_.reset();
    if (io_) io_->stop();
    if (thread_.joinable()) thread_.join();
    if (io_) io_->restart();
    if (port_ && port_->is_open()) {
        error_code ec;
        port_->close(ec);
    }

    {
        std::lock_guard<std::mutex> lk(bufMx_);
        readBuffer_.clear();
    }
    bytesToWrite_.store(0);
}

bool AsioBackend::isOpen() const {
    return port_ && port_->is_open();
}

// ---- config ---------------------------------------------------------------

void AsioBackend::applyConfig() {
    if (!port_ || !port_->is_open()) return;
    error_code ec;
    port_->set_option(asio::serial_port::baud_rate(static_cast<unsigned>(baud_)), ec);
    port_->set_option(asio::serial_port::character_size(static_cast<unsigned>(dataBits_)), ec);

    using Par = asio::serial_port::parity;
    switch (parity_) {
        case Parity::NoParity:   port_->set_option(Par(Par::none), ec); break;
        case Parity::EvenParity: port_->set_option(Par(Par::even), ec); break;
        case Parity::OddParity:  port_->set_option(Par(Par::odd),  ec); break;
        default: break; // space/mark handled in applyNativeTimeouts() via termios
    }

    using Stop = asio::serial_port::stop_bits;
    switch (stopBits_) {
        case StopBits::OneStop:        port_->set_option(Stop(Stop::one), ec); break;
        case StopBits::TwoStop:        port_->set_option(Stop(Stop::two), ec); break;
        case StopBits::OneAndHalfStop: port_->set_option(Stop(Stop::onepointfive), ec); break;
    }

    using Flow = asio::serial_port::flow_control;
    switch (flow_) {
        case FlowControl::NoFlowControl:   port_->set_option(Flow(Flow::none), ec); break;
        case FlowControl::HardwareControl: port_->set_option(Flow(Flow::hardware), ec); break;
        case FlowControl::SoftwareControl: port_->set_option(Flow(Flow::software), ec); break;
    }
}

void AsioBackend::applyNativeTimeouts() {
    if (!port_ || !port_->is_open()) return;
#if defined(CPS_BACKEND_POSIX)
    int fd = port_->native_handle();
    struct termios t{};
    if (tcgetattr(fd, &t) == 0) {
        // Raw mode: pass binary data through untouched. The port defaults to
        // canonical (cooked) mode, which interprets control bytes inside a payload:
        // 0x04 -> EOF (read returns 0 -> asio error -> read loop stops),
        // 0x03/0x1A -> SIGINT/SIGTSTP, 0x13/0x11 -> XOFF/XON, \r<->\n mangling.
        // A plain-text response survives (lines flush on \n), a binary one kills the
        // read. Char size / parity are left as asio set them (c_cflag untouched).
        t.c_iflag &= ~(IGNBRK | BRKINT | PARMRK | ISTRIP | INLCR | IGNCR | ICRNL
                       | IXON | IXOFF | IXANY);
        t.c_lflag &= ~(ECHO | ECHONL | ICANON | ISIG | IEXTEN);
        t.c_oflag &= ~OPOST;
        t.c_cc[VMIN]  = 0;
        t.c_cc[VTIME] = 1; // 0.1s inter-byte timeout -> responsive readyRead
        // space / mark parity (Linux-specific CMSPAR)
#ifdef CMSPAR
        if (parity_ == Parity::SpaceParity) {
            t.c_cflag |= (PARENB | CMSPAR);
            t.c_cflag &= ~PARODD;
        } else if (parity_ == Parity::MarkParity) {
            t.c_cflag |= (PARENB | CMSPAR | PARODD);
        }
#endif
        tcsetattr(fd, TCSANOW, &t);
    }
#else
    HANDLE h = port_->native_handle();
    COMMTIMEOUTS to{};
    to.ReadIntervalTimeout         = 50; // ms gap between bytes
    to.ReadTotalTimeoutMultiplier  = 0;
    to.ReadTotalTimeoutConstant    = 0;
    to.WriteTotalTimeoutMultiplier = 0;
    to.WriteTotalTimeoutConstant   = 0;
    SetCommTimeouts(h, &to);
    // space / mark parity on windows
    if (parity_ == Parity::SpaceParity || parity_ == Parity::MarkParity) {
        DCB dcb{}; dcb.DCBlength = sizeof(DCB);
        if (GetCommState(h, &dcb)) {
            dcb.fParity = TRUE;
            dcb.Parity  = (parity_ == Parity::SpaceParity) ? SPACEPARITY : MARKPARITY;
            SetCommState(h, &dcb);
        }
    }
#endif
}

bool AsioBackend::setBaudRate(std::int32_t rate) {
    baud_ = rate;
    if (!isOpen()) return true;
    error_code ec;
    port_->set_option(asio::serial_port::baud_rate(static_cast<unsigned>(rate)), ec);
    return !ec;
}

bool AsioBackend::setDataBits(DataBits b) {
    dataBits_ = b;
    if (!isOpen()) return true;
    error_code ec;
    port_->set_option(asio::serial_port::character_size(static_cast<unsigned>(b)), ec);
    return !ec;
}

bool AsioBackend::setParity(Parity p) {
    parity_ = p;
    applyConfig();
    applyNativeTimeouts();
    return true;
}

bool AsioBackend::setStopBits(StopBits s) {
    stopBits_ = s;
    applyConfig();
    return true;
}

bool AsioBackend::setFlowControl(FlowControl f) {
    flow_ = f;
    applyConfig();
    return true;
}

// ---- io -------------------------------------------------------------------

void AsioBackend::startRead() { doRead(); }

void AsioBackend::doRead() {
    if (!running_ || !port_ || !port_->is_open()) return;
    port_->async_read_some(asio::buffer(readScratch_),
        [this](const error_code& ec, std::size_t n) {
            if (ec) {
                if (running_) {
                    SerialError mapped = mapError(ec);
                    if (mapped != SerialError::NoError) notifyError(mapped);
                }
                running_ = false;
                return;
            }
            if (n) {
                {
                    std::lock_guard<std::mutex> lk(bufMx_);
                    readBuffer_.push(readScratch_.data(), n);
                }
                if (onReadyRead) onReadyRead();
            }
            doRead();
        });
}

std::int64_t AsioBackend::write(const void* data, std::size_t size) {
    if (!isOpen()) { setError(SerialError::NotOpenError); return -1; }
    if (size == 0) return 0;

    auto buf = std::make_shared<std::vector<unsigned char>>(
        static_cast<const unsigned char*>(data),
        static_cast<const unsigned char*>(data) + size);
    bytesToWrite_.fetch_add(static_cast<std::int64_t>(buf->size()));

    asio::post(*io_, [this, buf]() {
        asio::async_write(*port_, asio::buffer(*buf),
            [this, buf](const error_code& ec, std::size_t written) {
                bytesToWrite_.fetch_sub(static_cast<std::int64_t>(buf->size()));
                if (ec) {
                    SerialError mapped = mapError(ec);
                    if (mapped != SerialError::NoError) notifyError(mapped);
                } else if (onBytesWritten && written) {
                    onBytesWritten(static_cast<std::int64_t>(written));
                }
            });
    });
    return static_cast<std::int64_t>(size);
}

std::int64_t AsioBackend::read(void* out, std::size_t max) {
    if (!out) return 0;
    std::lock_guard<std::mutex> lk(bufMx_);
    return static_cast<std::int64_t>(readBuffer_.pop(static_cast<std::uint8_t*>(out), max));
}

std::int64_t AsioBackend::bytesAvailable() const {
    std::lock_guard<std::mutex> lk(bufMx_);
    return static_cast<std::int64_t>(readBuffer_.size());
}

std::int64_t AsioBackend::bytesToWrite() const {
    return bytesToWrite_.load();
}

bool AsioBackend::flush() {
    if (!isOpen()) return false;
#if defined(CPS_BACKEND_POSIX)
    return ::tcdrain(port_->native_handle()) == 0;
#else
    return ::FlushFileBuffers(port_->native_handle()) != 0;
#endif
}

bool AsioBackend::clear(Directions d) {
    if (!isOpen()) return false;
#if defined(CPS_BACKEND_POSIX)
    int q = 0;
    if (static_cast<int>(d) & static_cast<int>(Directions::Input))  q |= TCIFLUSH;
    if (static_cast<int>(d) & static_cast<int>(Directions::Output)) q |= TCOFLUSH;
    return ::tcflush(port_->native_handle(), q) == 0;
#else
    DWORD flags = 0;
    if (static_cast<int>(d) & static_cast<int>(Directions::Input))  flags |= PURGE_RXABORT | PURGE_RXCLEAR;
    if (static_cast<int>(d) & static_cast<int>(Directions::Output)) flags |= PURGE_TXABORT | PURGE_TXCLEAR;
    return ::PurgeComm(port_->native_handle(), flags) != 0;
#endif
}

// ---- error ----------------------------------------------------------------

SerialError AsioBackend::lastError() const { return error_.load(); }
void AsioBackend::clearError() { error_.store(SerialError::NoError); }
void AsioBackend::setError(SerialError e) { error_.store(e); }
void AsioBackend::notifyError(SerialError e) {
    setError(e);
    if (onError) onError(e);
}

} // namespace detail
} // namespace cps
