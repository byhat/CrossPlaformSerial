#include <cps/cps.h>

#include <cps/SerialPort.hpp>
#include <cps/SerialPortInfo.hpp>

#include <cstddef>
#include <cstring>
#include <new>
#include <string>
#include <vector>

#ifdef __ANDROID__
namespace cps {
namespace detail {
cps::SerialError androidInit(void *java_vm, void *context);
}
} // namespace cps
#endif

// ---- enum value lock ------------------------------------------------------

#define CPS_ENUM_CHECK(c_val, cpp_val) \
    static_assert(static_cast<int>(c_val) == static_cast<int>(cpp_val), "C/C++ enum value mismatch")

CPS_ENUM_CHECK(CPS_DATA_5, cps::DataBits::Data5);
CPS_ENUM_CHECK(CPS_DATA_6, cps::DataBits::Data6);
CPS_ENUM_CHECK(CPS_DATA_7, cps::DataBits::Data7);
CPS_ENUM_CHECK(CPS_DATA_8, cps::DataBits::Data8);

CPS_ENUM_CHECK(CPS_PAR_NONE, cps::Parity::NoParity);
CPS_ENUM_CHECK(CPS_PAR_EVEN, cps::Parity::EvenParity);
CPS_ENUM_CHECK(CPS_PAR_ODD, cps::Parity::OddParity);
CPS_ENUM_CHECK(CPS_PAR_SPACE, cps::Parity::SpaceParity);
CPS_ENUM_CHECK(CPS_PAR_MARK, cps::Parity::MarkParity);

CPS_ENUM_CHECK(CPS_STOP_ONE, cps::StopBits::OneStop);
CPS_ENUM_CHECK(CPS_STOP_ONE_HALF, cps::StopBits::OneAndHalfStop);
CPS_ENUM_CHECK(CPS_STOP_TWO, cps::StopBits::TwoStop);

CPS_ENUM_CHECK(CPS_FLOW_NONE, cps::FlowControl::NoFlowControl);
CPS_ENUM_CHECK(CPS_FLOW_HARDWARE, cps::FlowControl::HardwareControl);
CPS_ENUM_CHECK(CPS_FLOW_SOFTWARE, cps::FlowControl::SoftwareControl);

CPS_ENUM_CHECK(CPS_DIR_INPUT, cps::Directions::Input);
CPS_ENUM_CHECK(CPS_DIR_OUTPUT, cps::Directions::Output);
CPS_ENUM_CHECK(CPS_DIR_ALL, cps::Directions::AllDirections);

CPS_ENUM_CHECK(CPS_MODE_READ, cps::OpenMode::ReadOnly);
CPS_ENUM_CHECK(CPS_MODE_WRITE, cps::OpenMode::WriteOnly);
CPS_ENUM_CHECK(CPS_MODE_READWRITE, cps::OpenMode::ReadWrite);

CPS_ENUM_CHECK(CPS_ERR_NONE, cps::SerialError::NoError);
CPS_ENUM_CHECK(CPS_ERR_DEVICE_NOT_FOUND, cps::SerialError::DeviceNotFoundError);
CPS_ENUM_CHECK(CPS_ERR_PERMISSION, cps::SerialError::PermissionError);
CPS_ENUM_CHECK(CPS_ERR_OPEN, cps::SerialError::OpenError);
CPS_ENUM_CHECK(CPS_ERR_PARITY, cps::SerialError::ParityError);
CPS_ENUM_CHECK(CPS_ERR_FRAMING, cps::SerialError::FramingError);
CPS_ENUM_CHECK(CPS_ERR_WRITE, cps::SerialError::WriteError);
CPS_ENUM_CHECK(CPS_ERR_READ, cps::SerialError::ReadError);
CPS_ENUM_CHECK(CPS_ERR_RESOURCE, cps::SerialError::ResourceError);
CPS_ENUM_CHECK(CPS_ERR_UNSUPPORTED_OPERATION, cps::SerialError::UnsupportedOperationError);
CPS_ENUM_CHECK(CPS_ERR_UNKNOWN, cps::SerialError::UnknownError);
CPS_ENUM_CHECK(CPS_ERR_TIMEOUT, cps::SerialError::TimeoutError);
CPS_ENUM_CHECK(CPS_ERR_NOT_OPEN, cps::SerialError::NotOpenError);

// ---- helpers --------------------------------------------------------------

namespace {
const char *dupStr(const std::string &s)
{
    char *p = new char[s.size() + 1];
    std::memcpy(p, s.c_str(), s.size() + 1);
    return p;
}

// Header used to recover the count in cps_free_ports().
struct PortInfoBlock
{
    int count;
    cps_PortInfo entries[1];
};
} // namespace

// Completes the opaque type declared in <cps/cps.h>.
struct cps_SerialPort
{
    cps::SerialPort port;
    void *user_data = nullptr;
};

// ---- enumeration ----------------------------------------------------------

extern "C" CPS_API cps_PortInfo *cps_available_ports(int *out_count)
{
    std::vector<cps::SerialPortInfo> v = cps::SerialPortInfo::availablePorts();
    int n = static_cast<int>(v.size());
    if (out_count)
        *out_count = n;
    if (n == 0)
        return nullptr;

    const std::size_t bytes = sizeof(PortInfoBlock)
                              + sizeof(cps_PortInfo) * (static_cast<std::size_t>(n) - 1);
    void *mem = ::operator new(bytes);
    auto *block = static_cast<PortInfoBlock *>(mem);
    block->count = n;
    for (int i = 0; i < n; ++i) {
        cps_PortInfo &e = block->entries[i];
        e.port_name = dupStr(v[static_cast<std::size_t>(i)].portName());
        e.description = dupStr(v[static_cast<std::size_t>(i)].description());
        e.manufacturer = dupStr(v[static_cast<std::size_t>(i)].manufacturer());
        e.serial_number = dupStr(v[static_cast<std::size_t>(i)].serialNumber());
        e.system_location = dupStr(v[static_cast<std::size_t>(i)].systemLocation());
        e.vendor_id = v[static_cast<std::size_t>(i)].vendorIdentifier();
        e.product_id = v[static_cast<std::size_t>(i)].productIdentifier();
    }
    return block->entries;
}

extern "C" CPS_API void cps_free_ports(cps_PortInfo *ports)
{
    if (!ports)
        return;
    PortInfoBlock *block = reinterpret_cast<PortInfoBlock *>(reinterpret_cast<char *>(ports)
                                                             - offsetof(PortInfoBlock, entries));
    for (int i = 0; i < block->count; ++i) {
        delete[] block->entries[i].port_name;
        delete[] block->entries[i].description;
        delete[] block->entries[i].manufacturer;
        delete[] block->entries[i].serial_number;
        delete[] block->entries[i].system_location;
    }
    ::operator delete(block);
}

// ---- lifecycle ------------------------------------------------------------

extern "C" CPS_API cps_SerialPort *cps_serial_new(const char *port_name)
{
    auto *h = new cps_SerialPort;
    if (port_name)
        h->port.setPortName(port_name);
    return h;
}

extern "C" CPS_API void cps_serial_free(cps_SerialPort *port)
{
    delete port;
}

// ---- config + io ----------------------------------------------------------

extern "C" CPS_API cps_SerialError cps_serial_set_port_name(cps_SerialPort *p, const char *name)
{
    if (!p || !name)
        return CPS_ERR_UNKNOWN;
    p->port.setPortName(name);
    return CPS_ERR_NONE;
}

extern "C" CPS_API cps_SerialError cps_serial_open(cps_SerialPort *p, cps_OpenMode mode)
{
    if (!p)
        return CPS_ERR_UNKNOWN;
    bool ok = p->port.open(static_cast<cps::OpenMode>(mode));
    return ok ? CPS_ERR_NONE : static_cast<cps_SerialError>(p->port.error());
}

extern "C" CPS_API void cps_serial_close(cps_SerialPort *p)
{
    if (p)
        p->port.close();
}

extern "C" CPS_API int cps_serial_is_open(cps_SerialPort *p)
{
    return (p && p->port.isOpen()) ? 1 : 0;
}

extern "C" CPS_API long cps_serial_write(cps_SerialPort *p, const void *data, long size)
{
    if (!p)
        return -1;
    std::int64_t n = p->port.write(data, static_cast<std::size_t>(size));
    return static_cast<long>(n);
}

extern "C" CPS_API long cps_serial_read(cps_SerialPort *p, void *buf, long max_size)
{
    if (!p)
        return -1;
    std::int64_t n = p->port.read(buf, static_cast<std::size_t>(max_size));
    return static_cast<long>(n);
}

extern "C" CPS_API cps_SerialError cps_serial_set_baud_rate(cps_SerialPort *p,
                                                            int rate,
                                                            cps_Directions)
{
    if (!p)
        return CPS_ERR_UNKNOWN;
    bool ok = p->port.setBaudRate(rate);
    return ok ? CPS_ERR_NONE : static_cast<cps_SerialError>(p->port.error());
}

extern "C" CPS_API cps_SerialError cps_serial_set_data_bits(cps_SerialPort *p, cps_DataBits bits)
{
    if (!p)
        return CPS_ERR_UNKNOWN;
    bool ok = p->port.setDataBits(static_cast<cps::DataBits>(bits));
    return ok ? CPS_ERR_NONE : static_cast<cps_SerialError>(p->port.error());
}

extern "C" CPS_API cps_SerialError cps_serial_set_parity(cps_SerialPort *p, cps_Parity parity)
{
    if (!p)
        return CPS_ERR_UNKNOWN;
    bool ok = p->port.setParity(static_cast<cps::Parity>(parity));
    return ok ? CPS_ERR_NONE : static_cast<cps_SerialError>(p->port.error());
}

extern "C" CPS_API cps_SerialError cps_serial_set_stop_bits(cps_SerialPort *p, cps_StopBits bits)
{
    if (!p)
        return CPS_ERR_UNKNOWN;
    bool ok = p->port.setStopBits(static_cast<cps::StopBits>(bits));
    return ok ? CPS_ERR_NONE : static_cast<cps_SerialError>(p->port.error());
}

extern "C" CPS_API cps_SerialError cps_serial_set_flow_control(cps_SerialPort *p,
                                                               cps_FlowControl flow)
{
    if (!p)
        return CPS_ERR_UNKNOWN;
    bool ok = p->port.setFlowControl(static_cast<cps::FlowControl>(flow));
    return ok ? CPS_ERR_NONE : static_cast<cps_SerialError>(p->port.error());
}

extern "C" CPS_API long cps_serial_bytes_available(cps_SerialPort *p)
{
    if (!p)
        return 0;
    return static_cast<long>(p->port.bytesAvailable());
}

extern "C" CPS_API long cps_serial_bytes_to_write(cps_SerialPort *p)
{
    if (!p)
        return 0;
    return static_cast<long>(p->port.bytesToWrite());
}

extern "C" CPS_API cps_SerialError cps_serial_flush(cps_SerialPort *p)
{
    if (!p)
        return CPS_ERR_UNKNOWN;
    return p->port.flush() ? CPS_ERR_NONE : static_cast<cps_SerialError>(p->port.error());
}

extern "C" CPS_API cps_SerialError cps_serial_clear(cps_SerialPort *p, cps_Directions dir)
{
    if (!p)
        return CPS_ERR_UNKNOWN;
    return p->port.clear(static_cast<cps::Directions>(dir))
               ? CPS_ERR_NONE
               : static_cast<cps_SerialError>(p->port.error());
}

extern "C" CPS_API cps_SerialError cps_serial_error(cps_SerialPort *p)
{
    if (!p)
        return CPS_ERR_UNKNOWN;
    return static_cast<cps_SerialError>(p->port.error());
}

extern "C" CPS_API void cps_serial_clear_error(cps_SerialPort *p)
{
    if (p)
        p->port.clearError();
}

// ---- callbacks ------------------------------------------------------------

extern "C" CPS_API void cps_serial_set_callbacks(cps_SerialPort *p,
                                                 const cps_Callbacks *cbs,
                                                 void *user_data)
{
    if (!p)
        return;
    p->user_data = user_data;
    cps::SerialPort::Callbacks cpp;
    if (cbs) {
        if (cbs->on_ready_read) {
            void *u = user_data;
            auto fn = cbs->on_ready_read;
            cpp.onReadyRead = [u, fn]() { fn(u); };
        }
        if (cbs->on_bytes_written) {
            void *u = user_data;
            auto fn = cbs->on_bytes_written;
            cpp.onBytesWritten = [u, fn](std::int64_t n) { fn(u, static_cast<long>(n)); };
        }
        if (cbs->on_error) {
            void *u = user_data;
            auto fn = cbs->on_error;
            cpp.onError = [u, fn](cps::SerialError e) { fn(u, static_cast<cps_SerialError>(e)); };
        }
    }
    p->port.setCallbacks(std::move(cpp));
}

// ---- android init ---------------------------------------------------------

extern "C" CPS_API cps_SerialError cps_serial_init_android(void *java_vm, void *android_context)
{
#ifdef __ANDROID__
    return static_cast<cps_SerialError>(cps::detail::androidInit(java_vm, android_context));
#else
    (void) java_vm;
    (void) android_context;
    return CPS_ERR_UNSUPPORTED_OPERATION;
#endif
}

// ---- misc -----------------------------------------------------------------

extern "C" CPS_API const char *cps_version(void)
{
    return "1.0.0";
}

extern "C" CPS_API const char *cps_error_string(cps_SerialError err)
{
    switch (err) {
    case CPS_ERR_NONE:
        return "no error";
    case CPS_ERR_DEVICE_NOT_FOUND:
        return "device not found";
    case CPS_ERR_PERMISSION:
        return "permission denied";
    case CPS_ERR_OPEN:
        return "failed to open device";
    case CPS_ERR_PARITY:
        return "parity error";
    case CPS_ERR_FRAMING:
        return "framing error";
    case CPS_ERR_WRITE:
        return "write error";
    case CPS_ERR_READ:
        return "read error";
    case CPS_ERR_RESOURCE:
        return "resource error";
    case CPS_ERR_UNSUPPORTED_OPERATION:
        return "unsupported operation";
    case CPS_ERR_UNKNOWN:
        return "unknown error";
    case CPS_ERR_TIMEOUT:
        return "timeout";
    case CPS_ERR_NOT_OPEN:
        return "port not open";
    }
    return "unknown error";
}
