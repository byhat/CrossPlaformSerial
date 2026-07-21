# Qt6 QML terminal example that consumes cps ONLY through the stable C ABI
# (<cps/cps.h>). No cps C++ class is instantiated anywhere, so this example is
# ABI-portable (no compiler/STL compatibility requirement vs. how cps was built).
#
# Built only when Qt6 (>= 6.2) with Core/Gui/Quick is found (the guard lives in
# examples/CMakeLists.txt). Links the in-tree `cps` target, so build cps first.

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

qt_add_executable(cps_qt_serial_cabi
    main.cpp
    cabicontroller.cpp
    cabicontroller.h
    portlistmodel_cabi.cpp
    portlistmodel_cabi.h
)

qt_add_resources(cps_qt_serial_cabi "cps_qt_cabi_qml"
    PREFIX "/"
    FILES Main.qml)

target_link_libraries(cps_qt_serial_cabi PRIVATE
    cps
    Qt6::Core
    Qt6::Gui
    Qt6::Quick)

set_target_properties(cps_qt_serial_cabi PROPERTIES
    WIN32_EXECUTABLE OFF   # keep the console for status/error output
    MACOSX_BUNDLE ON)
