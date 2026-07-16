// Lists available serial ports using the C++ API.
#include <cps/SerialPortInfo.hpp>

#include <iomanip>
#include <iostream>

#ifdef _WIN32
#  include <windows.h>
#endif

int main() {
#ifdef _WIN32
    SetConsoleOutputCP(CP_UTF8);
#endif
    const auto ports = cps::SerialPortInfo::availablePorts();
    std::cout << "Found " << ports.size() << " serial port(s):\n";
    for (const auto& p : ports) {
        std::cout << "  " << std::left << std::setw(20) << p.portName()
                  << " vid=0x" << std::hex << std::setw(4) << std::setfill('0')
                  << p.vendorIdentifier()
                  << " pid=0x" << std::setw(4) << p.productIdentifier()
                  << std::dec << std::setfill(' ')
                  << "  " << p.description()
                  << "  [" << p.manufacturer() << "]\n";
    }
    return 0;
}
