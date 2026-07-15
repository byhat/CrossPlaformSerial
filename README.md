# cps — Cross-Platform Serial Library

A small C++17 serial-port library for **Linux, Windows, and Android** with a stable
**C ABI**. No Qt dependency inside.

- **Desktop** (Linux/Windows): backed by header-only **boost::asio**.
- **Android**: backed by **usb-serial-for-android**, compiled to a `classes.dex` that
  is **embedded into `libcps.so`** and loaded at runtime via `DexClassLoader`. The
  consumer never compiles or links Java.
- Builds as **shared** (`libcps.so` / `cps.dll`) or **static** (`libcps.a` / `cps.lib`).
- Two C++ classes: `cps::SerialPort` and `cps::SerialPortInfo` (mirrors
  `QSerialPortInfo` — port name, description, manufacturer, serial, VID/PID).
- C ABI (`cps_*` in `cps.h`) for use from C or any language.

## Platforms

| Platform | Backend |
|----------|---------|
| Linux    | boost::asio (termios under the hood) |
| Windows  | boost::asio (Win32 DCB/COMMTIMEOUTS) |
| Android  | JNI → usb-serial-for-android (embedded DEX) |

## Build

### Desktop (Linux / Windows), shared
```sh
cmake -S . -B build -DCPS_BUILD_SHARED=ON
cmake --build build
```
Static:
```sh
cmake -S . -B build-static -DCPS_BUILD_SHARED=OFF
cmake --build build-static
```
Examples are built by default (`cpp_list_ports`, `c_minimal`).

### Go (cgo, via the C ABI)
A self-contained cgo example in `examples/go/` links `libcps.so` and uses the C ABI
(including a real C -> Go `readyRead` callback). Build the `.so` first, then:
```sh
cd examples/go
go run . [/dev/ttyUSB0]
```
The cgo directives use `-rpath` to find `libcps.so`, so no `LD_LIBRARY_PATH` is needed.
See `examples/go/README.md`.

### Android (NDK + SDK required)
The Android build compiles the vendored `third_party/usb-serial-for-android` plus the
JNI bridge in `android/java/` into `classes.dex` with `javac` + `d8` and embeds it.
Populate the vendored library first (see `third_party/usb-serial-for-android/README.md`):
```sh
git submodule add https://github.com/mik3y/usb-serial-for-android.git \
  third_party/usb-serial-for-android
```
Then:
```sh
cmake -S . -B build-android \
  -DCMAKE_TOOLCHAIN_FILE=$ANDROID_NDK/build/cmake/android.toolchain.cmake \
  -DANDROID_ABI=arm64-v8a -DANDROID_PLATFORM=android-21 \
  -DANDROID_SDK_ROOT=$ANDROID_SDK_ROOT
cmake --build build-android
```
`CPS_ANDROID_MIN_API` (default `21`) controls the `d8 --min-api` level.

## Usage (C++)
```cpp
#include <cps/SerialPort.hpp>
#include <cps/SerialPortInfo.hpp>

cps::SerialPortInfo info = cps::SerialPortInfo::availablePorts().front();
cps::SerialPort port(info);
port.setBaudRate(115200);
port.setCallbacks({ /*onReadyRead=*/[&]{ auto data = port.readAll(); /* ... */ } });
port.open();
port.write(someBytes);
```

## Usage (C ABI)
```c
#include <cps/cps.h>

cps_SerialPort* p = cps_serial_new("/dev/ttyUSB0");
cps_serial_set_baud_rate(p, 9600, CPS_DIR_ALL);
cps_Callbacks cb = { .on_ready_read = my_ready_read };
cps_serial_set_callbacks(p, &cb, p);
if (cps_serial_open(p, CPS_MODE_READWRITE) == CPS_ERR_NONE) {
    cps_serial_write(p, "hi", 2);
}
```

## Android consumer contract (Qt-free)
1. Bundle `libcps.so` for the chosen ABIs in the APK.
2. Call `System.loadLibrary("cps")` once from your app's Java/Kotlin/Activity
   (standard for any JNI library — this is *not* "linking Java files"). This triggers
   `JNI_OnLoad`.
3. Once before first use, call `cps_serial_init_android(NULL, <android Context>)`.
   From a Qt for Android app pass `QtAndroid::androidContext()`; from a plain app,
   pass your `Activity`/`Context`.

> **Threading:** user callbacks (`onReadyRead`, `onBytesWritten`, `onError`) are
> invoked from the library's internal worker thread. Marshal to your own thread if
> needed (e.g. Qt: `QMetaObject::invokeMethod`).

## Layout
```
include/cps/        public headers (cps.h C ABI + C++ classes + enums + export macro)
src/                implementation (facades, fast-pimpl, backends, C ABI)
src/detail/         AsioBackend (desktop), AndroidBackend (JNI), ReadBuffer, enumeration
android/java/       JNI bridge (CpsUsbSerial.java) + CpsPortInfo.java
third_party/        usb-serial-for-android (vendored on Android)
cmake/              dex build/embed + package config
examples/           cpp_list_ports, c_minimal.c, go/ (cgo via the C ABI), android_snippet.cpp
```

## License
LGPL-2.1. See `LICENSE` and `NOTICE`.
