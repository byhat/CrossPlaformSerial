#ifndef CPS_DETAIL_PORT_ENUMERATOR_HPP
#define CPS_DETAIL_PORT_ENUMERATOR_HPP

#include <cstdint>
#include <string>
#include <vector>

namespace cps {
namespace detail {

/** Raw, platform-collected port info (converted to cps::SerialPortInfo by the class). */
struct RawPortInfo {
    std::string  portName;
    std::string  description;
    std::string  manufacturer;
    std::string  serialNumber;
    std::string  systemLocation;
    std::uint16_t vendorId  = 0;
    std::uint16_t productId = 0;
    bool          hasUsbIds = false;
};

/** Platform-specific enumeration of available serial ports. */
std::vector<RawPortInfo> enumeratePorts();

} // namespace detail
} // namespace cps

#endif // CPS_DETAIL_PORT_ENUMERATOR_HPP
