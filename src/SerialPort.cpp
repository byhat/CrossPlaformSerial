#include <cps/SerialPort.hpp>
#include "detail/SerialBackend.hpp"
#include "detail/ReadBuffer.hpp"

#ifdef __ANDROID__
#  include "detail/AndroidBackend.hpp"
#else
#  include "detail/AsioBackend.hpp"
#endif

#include <new>
#include <utility>
#include <cstring>

namespace cps {

class SerialPort::Impl {
public:
    std::unique_ptr<detail::SerialBackend> backend;
    std::string portName;
    std::int32_t  baudRate_ = 9600;
    DataBits      dataBits_ = DataBits::Data8;
    Parity        parity_   = Parity::NoParity;
    StopBits      stopBits_ = StopBits::OneStop;
    FlowControl   flow_     = FlowControl::NoFlowControl;
    OpenMode      mode_     = OpenMode::ReadWrite;
    SerialPort::Callbacks cbs;

    Impl() {
#ifdef __ANDROID__
        backend = std::make_unique<detail::AndroidBackend>();
#else
        backend = std::make_unique<detail::AsioBackend>();
#endif
        wire();
    }

    void wire() {
        backend->onReadyRead    = [this]() { if (cbs.onReadyRead)    cbs.onReadyRead(); };
        backend->onBytesWritten = [this](std::int64_t n) { if (cbs.onBytesWritten) cbs.onBytesWritten(n); };
        backend->onError        = [this](SerialError e) { if (cbs.onError) cbs.onError(e); };
    }

    void pushConfig() {
        backend->setBaudRate(baudRate_);
        backend->setDataBits(dataBits_);
        backend->setParity(parity_);
        backend->setStopBits(stopBits_);
        backend->setFlowControl(flow_);
    }
};

// ---- impl accessors -------------------------------------------------------

SerialPort::Impl& SerialPort::impl() noexcept {
    return *std::launder(reinterpret_cast<Impl*>(mStorage));
}
const SerialPort::Impl& SerialPort::impl() const noexcept {
    return *std::launder(reinterpret_cast<const Impl*>(mStorage));
}

// ---- special members ------------------------------------------------------

SerialPort::SerialPort() {
    static_assert(sizeof(Impl) <= kImplSize, "SerialPort: bump kImplSize");
    static_assert(alignof(Impl) <= kImplAlign, "SerialPort: bump kImplAlign");
    ::new (mStorage) Impl();
}
SerialPort::SerialPort(const std::string& portName) {
    ::new (mStorage) Impl();
    impl().portName = portName;
}
SerialPort::SerialPort(const SerialPortInfo& info) {
    ::new (mStorage) Impl();
    impl().portName = info.portName();
}
SerialPort::~SerialPort() {
    impl().~Impl();
}

SerialPort::SerialPort(SerialPort&& o) noexcept {
    ::new (mStorage) Impl(std::move(o.impl()));
}
SerialPort& SerialPort::operator=(SerialPort&& o) noexcept {
    if (this != &o) impl() = std::move(o.impl());
    return *this;
}

// ---- port name / open -----------------------------------------------------

void SerialPort::setPortName(const std::string& name) { impl().portName = name; }
std::string SerialPort::portName() const { return impl().portName; }

bool SerialPort::open(OpenMode mode) {
    auto& i = impl();
    i.mode_ = mode;
    i.pushConfig();
    return i.backend->open(i.portName, mode);
}
void SerialPort::close() { impl().backend->close(); }
bool SerialPort::isOpen() const { return impl().backend->isOpen(); }

// ---- io -------------------------------------------------------------------

std::int64_t SerialPort::write(const void* data, std::size_t size) {
    return impl().backend->write(data, size);
}
std::int64_t SerialPort::write(const std::vector<std::uint8_t>& data) {
    return data.empty() ? 0 : impl().backend->write(data.data(), data.size());
}
std::int64_t SerialPort::read(void* buf, std::size_t maxSize) {
    return impl().backend->read(buf, maxSize);
}
std::vector<std::uint8_t> SerialPort::readAll() {
    std::vector<std::uint8_t> out;
    auto& i = impl();
    std::int64_t n = i.backend->bytesAvailable();
    if (n <= 0) return out;
    out.resize(static_cast<std::size_t>(n));
    std::int64_t got = i.backend->read(out.data(), out.size());
    if (got <= 0) out.clear();
    else out.resize(static_cast<std::size_t>(got));
    return out;
}

// ---- config ---------------------------------------------------------------

bool SerialPort::setBaudRate(std::int32_t rate, Directions) {
    impl().baudRate_ = rate;
    return impl().backend->setBaudRate(rate);
}
std::int32_t SerialPort::baudRate(Directions) const { return impl().baudRate_; }

bool SerialPort::setDataBits(DataBits b) {
    impl().dataBits_ = b;
    return impl().backend->setDataBits(b);
}
DataBits SerialPort::dataBits() const { return impl().dataBits_; }

bool SerialPort::setParity(Parity p) {
    impl().parity_ = p;
    return impl().backend->setParity(p);
}
Parity SerialPort::parity() const { return impl().parity_; }

bool SerialPort::setStopBits(StopBits s) {
    impl().stopBits_ = s;
    return impl().backend->setStopBits(s);
}
StopBits SerialPort::stopBits() const { return impl().stopBits_; }

bool SerialPort::setFlowControl(FlowControl f) {
    impl().flow_ = f;
    return impl().backend->setFlowControl(f);
}
FlowControl SerialPort::flowControl() const { return impl().flow_; }

// ---- buffer / error -------------------------------------------------------

bool SerialPort::clear(Directions d) { return impl().backend->clear(d); }
bool SerialPort::flush()             { return impl().backend->flush(); }
std::int64_t SerialPort::bytesAvailable() const { return impl().backend->bytesAvailable(); }
std::int64_t SerialPort::bytesToWrite() const   { return impl().backend->bytesToWrite(); }
SerialError SerialPort::error() const           { return impl().backend->lastError(); }
void SerialPort::clearError()                   { impl().backend->clearError(); }

void SerialPort::setCallbacks(Callbacks cbs) {
    impl().cbs = std::move(cbs);
}

} // namespace cps
