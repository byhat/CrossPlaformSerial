#include "detail/PortEnumerator.hpp"
#include <cps/SerialPortInfo.hpp>

#include <new>
#include <utility>

namespace cps {

class SerialPortInfo::Impl
{
public:
    std::string portName;
    std::string description;
    std::string manufacturer;
    std::string serialNumber;
    std::string systemLocation;
    std::uint16_t vendorId = 0;
    std::uint16_t productId = 0;
    bool null = true;

    Impl() = default;
    Impl(std::string pn,
         std::string desc,
         std::string manuf,
         std::string sn,
         std::uint16_t vid,
         std::uint16_t pid,
         std::string sysloc)
        : portName(std::move(pn))
        , description(std::move(desc))
        , manufacturer(std::move(manuf))
        , serialNumber(std::move(sn))
        , systemLocation(std::move(sysloc))
        , vendorId(vid)
        , productId(pid)
        , null(false)
    {}
};

// ---- impl accessors -------------------------------------------------------

SerialPortInfo::Impl &SerialPortInfo::impl() noexcept
{
    return *std::launder(reinterpret_cast<Impl *>(mStorage));
}
const SerialPortInfo::Impl &SerialPortInfo::impl() const noexcept
{
    return *std::launder(reinterpret_cast<const Impl *>(mStorage));
}

// ---- special members ------------------------------------------------------

SerialPortInfo::SerialPortInfo()
{
    static_assert(sizeof(Impl) <= kImplSize, "SerialPortInfo: bump kImplSize");
    static_assert(alignof(Impl) <= kImplAlign, "SerialPortInfo: bump kImplAlign");
    ::new (mStorage) Impl();
}
SerialPortInfo::~SerialPortInfo()
{
    impl().~Impl();
}

SerialPortInfo::SerialPortInfo(const SerialPortInfo &o)
{
    ::new (mStorage) Impl(o.impl());
}
SerialPortInfo &SerialPortInfo::operator=(const SerialPortInfo &o)
{
    if (this != &o)
        impl() = o.impl();
    return *this;
}
SerialPortInfo::SerialPortInfo(SerialPortInfo &&o) noexcept
{
    ::new (mStorage) Impl(std::move(o.impl()));
}
SerialPortInfo &SerialPortInfo::operator=(SerialPortInfo &&o) noexcept
{
    if (this != &o)
        impl() = std::move(o.impl());
    return *this;
}

SerialPortInfo::SerialPortInfo(const std::string &portName,
                               const std::string &description,
                               const std::string &manufacturer,
                               const std::string &serialNumber,
                               std::uint16_t vendorId,
                               std::uint16_t productId,
                               const std::string &systemLocation)
{
    ::new (mStorage)
        Impl(portName, description, manufacturer, serialNumber, vendorId, productId, systemLocation);
}

void SerialPortInfo::swap(SerialPortInfo &other)
{
    impl().portName.swap(other.impl().portName);
    impl().description.swap(other.impl().description);
    impl().manufacturer.swap(other.impl().manufacturer);
    impl().serialNumber.swap(other.impl().serialNumber);
    impl().systemLocation.swap(other.impl().systemLocation);
    std::swap(impl().vendorId, other.impl().vendorId);
    std::swap(impl().productId, other.impl().productId);
    std::swap(impl().null, other.impl().null);
}

// ---- getters --------------------------------------------------------------

std::string SerialPortInfo::portName() const
{
    return impl().portName;
}
std::string SerialPortInfo::description() const
{
    return impl().description;
}
std::string SerialPortInfo::manufacturer() const
{
    return impl().manufacturer;
}
std::string SerialPortInfo::serialNumber() const
{
    return impl().serialNumber;
}
std::string SerialPortInfo::systemLocation() const
{
    return impl().systemLocation;
}
std::uint16_t SerialPortInfo::productIdentifier() const
{
    return impl().productId;
}
std::uint16_t SerialPortInfo::vendorIdentifier() const
{
    return impl().vendorId;
}
bool SerialPortInfo::isNull() const
{
    return impl().null;
}

std::vector<std::int32_t> SerialPortInfo::standardBaudRates() const
{
    return {50,     75,     110,     134,     150,     200,     300,     600,     1200,    1800,
            2400,   4800,   9600,    19200,   38400,   57600,   115200,  230400,  460800,  500000,
            576000, 921600, 1000000, 1152000, 1500000, 2000000, 2500000, 3000000, 3500000, 4000000};
}

// ---- enumeration ----------------------------------------------------------

std::vector<SerialPortInfo> SerialPortInfo::availablePorts()
{
    std::vector<detail::RawPortInfo> raw = detail::enumeratePorts();
    std::vector<SerialPortInfo> out;
    out.reserve(raw.size());
    for (const detail::RawPortInfo &r : raw) {
        out.push_back(SerialPortInfo(r.portName,
                                     r.description,
                                     r.manufacturer,
                                     r.serialNumber,
                                     r.vendorId,
                                     r.productId,
                                     r.systemLocation));
    }
    return out;
}

} // namespace cps
