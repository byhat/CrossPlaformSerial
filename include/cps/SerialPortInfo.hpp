#ifndef CPS_SERIAL_PORT_INFO_HPP
#define CPS_SERIAL_PORT_INFO_HPP

#include <cps/cps_export.h>
#include <cps/SerialTypes.hpp>

#include <cstdint>
#include <cstddef>
#include <string>
#include <vector>
#include <utility>

namespace cps {

/**
 * @brief Information about an available serial port (mirrors QSerialPortInfo).
 *
 * Provides port name, description, manufacturer, serial number, VID/PID and system
 * location. The implementation is platform-specific (native enumeration on desktop,
 * USB enumeration via JNI on Android) and hidden behind a fast-pimpl.
 */
class CPS_API SerialPortInfo {
public:
    SerialPortInfo();
    SerialPortInfo(const SerialPortInfo&);
    SerialPortInfo& operator=(const SerialPortInfo&);
    SerialPortInfo(SerialPortInfo&&) noexcept;
    SerialPortInfo& operator=(SerialPortInfo&&) noexcept;
    ~SerialPortInfo();

    std::string portName() const;
    std::string description() const;
    std::string manufacturer() const;
    std::string serialNumber() const;
    std::uint16_t productIdentifier() const;   // PID
    std::uint16_t vendorIdentifier() const;    // VID
    std::string systemLocation() const;
    std::vector<std::int32_t> standardBaudRates() const;
    bool isNull() const;
    void swap(SerialPortInfo& other);

    /** Query the platform for available serial devices. */
    static std::vector<SerialPortInfo> availablePorts();

private:
    /** Internal constructor used by availablePorts() to build an instance from raw data. */
    SerialPortInfo(const std::string& portName,
                   const std::string& description,
                   const std::string& manufacturer,
                   const std::string& serialNumber,
                   std::uint16_t vendorId,
                   std::uint16_t productId,
                   const std::string& systemLocation);

    class Impl;
    // Fast pimpl: Impl is placement-new'd into mStorage (no heap alloc, no indirection).
    static constexpr std::size_t kImplSize  = 256;   // bump if the static_assert in .cpp fires
    static constexpr std::size_t kImplAlign = alignof(std::max_align_t);
    alignas(kImplAlign) std::byte mStorage[kImplSize];
    Impl&       impl() noexcept;
    const Impl& impl() const noexcept;
};

} // namespace cps

#endif // CPS_SERIAL_PORT_INFO_HPP
