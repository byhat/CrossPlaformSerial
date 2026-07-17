# Qt6 QML terminal example. Built only when Qt6 (>= 6.2) with Core/Gui/Quick is found
# (the guard lives in examples/CMakeLists.txt). It links the in-tree `cps` target, so
# build cps first (shared or static) before configuring the examples.

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

qt_add_executable(cps_qt_serial
    main.cpp
    portlistmodel.cpp
    portlistmodel.h
    serialcontroller.cpp
    serialcontroller.h
)

qt_add_resources(cps_qt_serial "cps_qt_qml"
    PREFIX "/"
    FILES Main.qml)

target_link_libraries(cps_qt_serial PRIVATE
    cps
    Qt6::Core
    Qt6::Gui
    Qt6::Quick)

set_target_properties(cps_qt_serial PROPERTIES
    WIN32_EXECUTABLE OFF   # keep the console for status/error output
    MACOSX_BUNDLE ON)
