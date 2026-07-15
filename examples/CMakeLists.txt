# Examples are optional consumers of the cps library.

if(CPS_BUILD_EXAMPLES)
    # C++ example: list available ports
    add_executable(cpp_list_ports cpp_list_ports.cpp)
    target_link_libraries(cpp_list_ports PRIVATE cps)

    # C example: exercises the C ABI
    enable_language(C)
    add_executable(c_minimal c_minimal.c)
    target_link_libraries(c_minimal PRIVATE cps)
endif()
