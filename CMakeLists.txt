cmake_minimum_required(VERSION 3.16)

project(cps
    VERSION 1.0.0
    DESCRIPTION "Cross-platform serial port library (Linux/Windows/Android)"
    LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

include(GNUInstallDirs)

# ---- options ---------------------------------------------------------------

option(CPS_BUILD_SHARED  "Build cps as a shared library (otherwise static)" ON)
option(CPS_BUILD_EXAMPLES "Build the example programs" ON)
set(CPS_ANDROID_MIN_API "21" CACHE STRING "Android min API level for the embedded dex (Android only)")

# ---- target ----------------------------------------------------------------

set(CPS_LIB_TYPE STATIC)
if(CPS_BUILD_SHARED)
    set(CPS_LIB_TYPE SHARED)
endif()

add_library(cps ${CPS_LIB_TYPE}
    src/SerialPort.cpp
    src/SerialPortInfo.cpp
    src/CAbi.cpp
)

target_include_directories(cps
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
)

set_target_properties(cps PROPERTIES
    CXX_VISIBILITY_PRESET hidden
    VISIBILITY_INLINES_HIDDEN ON
    OUTPUT_NAME cps
    VERSION ${PROJECT_VERSION}
    SOVERSION 1
)

if(CPS_BUILD_SHARED)
    target_compile_definitions(cps PRIVATE cps_EXPORTS)
else()
    target_compile_definitions(cps PUBLIC CPS_STATIC_DEFINE)
endif()

if(ANDROID)
    # ---- Android: JNI bridge + embedded dex --------------------------------
    target_compile_definitions(cps PRIVATE CPS_PLATFORM_ANDROID)
    target_sources(cps PRIVATE src/detail/AndroidBackend.cpp)
    target_link_libraries(cps PRIVATE log android)

    include(${CMAKE_CURRENT_SOURCE_DIR}/cmake/cps_dex.cmake)
    cps_embed_dex(cps
        JAVA_DIR     "${CMAKE_CURRENT_SOURCE_DIR}/android/java"
        USB_SERIAL_DIR "${CMAKE_CURRENT_SOURCE_DIR}/third_party/usb-serial-for-android"
        GENERATED_DIR "${CMAKE_CURRENT_BINARY_DIR}/generated"
        MIN_API      "${CPS_ANDROID_MIN_API}")
else()
    # ---- Desktop (Linux/Windows): boost::asio -------------------------------
    target_sources(cps PRIVATE
        src/detail/AsioBackend.cpp
        src/detail/PortEnumerator.cpp)

    find_package(Boost)
    if(Boost_FOUND)
        if(TARGET Boost::headers)
            target_link_libraries(cps PRIVATE Boost::headers)
        else()
            target_include_directories(cps PRIVATE ${Boost_INCLUDE_DIRS})
        endif()
    endif()

    if(WIN32)
        target_link_libraries(cps PRIVATE setupapi ws2_32)
    endif()
endif()

# ---- examples --------------------------------------------------------------

if(CPS_BUILD_EXAMPLES)
    enable_testing()
    add_subdirectory(examples)
endif()

# ---- install + export ------------------------------------------------------

install(TARGETS cps
    EXPORT cpsTargets
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

install(DIRECTORY include/ DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})

install(EXPORT cpsTargets
    FILE cpsTargets.cmake
    NAMESPACE cps::
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/cps)

include(CMakePackageConfigHelpers)
write_basic_package_version_file(
    "${CMAKE_CURRENT_BINARY_DIR}/cpsConfigVersion.cmake"
    VERSION ${PROJECT_VERSION}
    COMPATIBILITY SameMajorVersion)

configure_package_config_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/cmake/cpsConfig.cmake.in"
    "${CMAKE_CURRENT_BINARY_DIR}/cpsConfig.cmake"
    INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/cps)

install(FILES
    "${CMAKE_CURRENT_BINARY_DIR}/cpsConfig.cmake"
    "${CMAKE_CURRENT_BINARY_DIR}/cpsConfigVersion.cmake"
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/cps)
