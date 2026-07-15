# cps_embed_dex(TARGET JAVA_DIR USB_SERIAL_DIR GENERATED_DIR MIN_API)
#
# Compiles the Java bridge + vendored usb-serial-for-android into classes.dex,
# then embeds the dex bytes as a C array compiled into the target.
#
# Requires ANDROID_SDK_ROOT to point at the Android SDK (with platforms/ and build-tools/).
cmake_minimum_required(VERSION 3.16)

function(cps_embed_dex)
    cmake_parse_arguments(ARG "" "TARGET;JAVA_DIR;USB_SERIAL_DIR;GENERATED_DIR;MIN_API" "" ${ARGN})

    set(DEX_OUT     "${ARG_GENERATED_DIR}/classes.dex")
    set(CLASSES_DIR "${ARG_GENERATED_DIR}/classes")
    set(JAR_OUT     "${ARG_GENERATED_DIR}/cps-classes.jar")
    set(GEN_CPP     "${ARG_GENERATED_DIR}/cps_classes_dex.cpp")

    # ---- discover toolchain --------------------------------------------------
    find_program(CPS_JAVAC NAMES javac)
    if(NOT CPS_JAVAC)
        message(FATAL_ERROR "javac not found; a JDK is required to build the Android dex.")
    endif()

    if(NOT ANDROID_SDK_ROOT)
        message(FATAL_ERROR "ANDROID_SDK_ROOT must be set for Android builds (need javac + d8 + android.jar).")
    endif()

    file(GLOB CPS_ANDROID_JARS
        "${ANDROID_SDK_ROOT}/platforms/android-*/android.jar")
    list(SORT CPS_ANDROID_JARS)
    list(GET CPS_ANDROID_JARS -1 CPS_ANDROID_JAR)
    if(NOT CPS_ANDROID_JAR)
        message(FATAL_ERROR "No android.jar under ${ANDROID_SDK_ROOT}/platforms/. Install an Android platform.")
    endif()

    file(GLOB CPS_D8_CANDIDATES "${ANDROID_SDK_ROOT}/build-tools/*/d8")
    list(SORT CPS_D8_CANDIDATES)
    list(GET CPS_D8_CANDIDATES -1 CPS_D8)
    if(NOT CPS_D8)
        message(FATAL_ERROR "d8 not found under ${ANDROID_SDK_ROOT}/build-tools/. Install Android build-tools.")
    endif()

    # ---- collect java sources ------------------------------------------------
    file(GLOB CPS_BRIDGE_JAVA    "${ARG_JAVA_DIR}/**/*.java" "${ARG_JAVA_DIR}/*.java")
    file(GLOB CPS_USBSERIAL_JAVA "${ARG_USB_SERIAL_DIR}/**/*.java")

    set(CPS_ALL_JAVA ${CPS_BRIDGE_JAVA} ${CPS_USBSERIAL_JAVA})
    if(NOT CPS_ALL_JAVA)
        message(WARNING "No Java sources found for dex embedding (bridge or usb-serial-for-android).")
    endif()

    # ---- compile java -> jar -------------------------------------------------
    add_custom_command(
        OUTPUT "${DEX_OUT}"
        COMMAND ${CMAKE_COMMAND} -E make_directory "${CLASSES_DIR}"
        COMMAND "${CPS_JAVAC}" -source 1.8 -target 1.8 -encoding UTF-8
                -bootclasspath "${CPS_ANDROID_JAR}"
                -d "${CLASSES_DIR}"
                ${CPS_ALL_JAVA}
        COMMAND ${CMAKE_COMMAND} -E tar cf "${JAR_OUT}" --format=zip .
                WORKING_DIRECTORY "${CLASSES_DIR}"
        COMMAND "${CPS_D8}" --min-api "${ARG_MIN_API}" --output "${ARG_GENERATED_DIR}" "${JAR_OUT}"
        DEPENDS ${CPS_ALL_JAVA}
        COMMENT "Building embedded classes.dex (cps Android bridge)"
        VERBATIM)

    # ---- embed dex bytes as a C array ---------------------------------------
    add_custom_command(
        OUTPUT "${GEN_CPP}"
        COMMAND ${CMAKE_COMMAND}
            -DCPS_DEX_IN="${DEX_OUT}"
            -DCPS_CPP_OUT="${GEN_CPP}"
            -P "${CMAKE_CURRENT_LIST_DIR}/cps_dex_embed.cmake"
        DEPENDS "${DEX_OUT}"
        COMMENT "Embedding classes.dex into cps_classes_dex.cpp"
        VERBATIM)

    target_sources(${ARG_TARGET} PRIVATE "${GEN_CPP}")
    target_include_directories(${ARG_TARGET} PRIVATE "${ARG_GENERATED_DIR}")
endfunction()
