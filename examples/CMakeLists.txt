# Examples are optional consumers of the cps library.

if(CPS_BUILD_EXAMPLES)
    # C++ example: list available ports
    add_executable(cpp_list_ports cpp_list_ports.cpp)
    target_link_libraries(cpp_list_ports PRIVATE cps)

    # C example: exercises the C ABI
    enable_language(C)
    add_executable(c_minimal c_minimal.c)
    target_link_libraries(c_minimal PRIVATE cps)

    # Qt6 QML terminal (only when Qt6 is available)
    find_package(Qt6 6.2 QUIET COMPONENTS Core Gui Quick)
    if(Qt6_FOUND)
        add_subdirectory(qt_serial)
    endif()
endif()
