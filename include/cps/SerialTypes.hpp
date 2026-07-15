#ifndef CPS_SERIAL_TYPES_HPP
#define CPS_SERIAL_TYPES_HPP

#include <cstdint>
#include <cstddef>

namespace cps {

/* NOTE: integer values below MUST stay in sync with the C enums in <cps/cps.h>.
 * A static_assert in CAbi.cpp locks this at compile time. */

enum class DataBits : int {
    Data5 = 5,
    Data6 = 6,
    Data7 = 7,
    Data8 = 8
};

enum class Parity : int {
    NoParity    = 0,
    EvenParity  = 1,
    OddParity   = 2,
    SpaceParity = 3,
    MarkParity  = 4
};

enum class StopBits : int {
    OneStop        = 0,
    OneAndHalfStop = 1,
    TwoStop        = 2
};

enum class FlowControl : int {
    NoFlowControl    = 0,
    HardwareControl  = 1,
    SoftwareControl  = 2
};

/* Bitmask-style directions. */
enum class Directions : int {
    Input        = 1,
    Output       = 2,
    AllDirections = 3
};

enum class OpenMode : int {
    ReadOnly  = 1,
    WriteOnly = 2,
    ReadWrite = 3
};

enum class SerialError : int {
    NoError                   = 0,
    DeviceNotFoundError,
    PermissionError,
    OpenError,
    ParityError,
    FramingError,
    WriteError,
    ReadError,
    ResourceError,
    UnsupportedOperationError,
    UnknownError,
    TimeoutError,
    NotOpenError
};

} // namespace cps

#endif // CPS_SERIAL_TYPES_HPP
