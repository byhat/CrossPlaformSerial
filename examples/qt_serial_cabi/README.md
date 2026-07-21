# cps Qt6 QML terminal — C ABI consumer

Same UI/feature set as `../qt_serial`, but it talks to cps **exclusively through the
stable C ABI** (`<cps/cps.h>`). No cps C++ class is instantiated anywhere — the only
cps symbol touched is the opaque handle `cps_SerialPort*` plus the `cps_serial_*` /
`cps_available_ports` C functions and the `cps_Callbacks` C struct.

That makes this variant **ABI-portable**: it does not require the consuming compiler/STL
to match the one cps was built with (unlike the C++ API in `../qt_serial`).

## What uses the C ABI

- `cabicontroller.cpp` — `cps_serial_new`/`cps_serial_free` for an opaque handle;
  `cps_serial_set_port_name`/`set_baud_rate`/`open`/`close`/`write`/`read`/`clear`;
  `cps_Callbacks` with `extern "C"` trampolines (`cps_cabi_ready_read`, `cps_cabi_error`)
  that forward into `CabiController::handleReadyRead/handleError` and marshal to the GUI
  thread via queued signals.
- `portlistmodel_cabi.cpp` — `cps_available_ports` / `cps_free_ports`.
- `main.cpp` — on Android, `cps_serial_init_android(vm, context)` before first use.

Everything else (dropdown, refresh, connect/disconnect, baud, accumulating receive area
with `[time] (N bytes):` prefix, clear, send + `\r\n` option) is identical to
`../qt_serial`.

## Build

Same as `../qt_serial` — point CMake at your Qt6 and build cps first. This example is
added automatically when Qt6 is found:

```powershell
cmake -S . -B build-qt -G "MinGW Makefiles" -DCMAKE_PREFIX_PATH=C:/Qt/6.9.0/mingw_64
cmake --build build-qt --target cps_qt_serial_cabi
```

## Run (Windows)

```powershell
$env:Path = "C:\Qt\6.9.0\mingw_64\bin;.\build-qt;$env:Path"
.\build-qt\examples\qt_serial_cabi\cps_qt_serial_cabi.exe
```

## Android / Linux

Cross-platform by construction: the `cps_serial_*` C API is the same on every platform
(asio backend on desktop, JNI backend on Android). On Android, `main.cpp` calls
`cps_serial_init_android(...)` (same pattern as `../qt_serial`); build the Qt app with
your Android kit — it will link the prebuilt `libcps.so` for the target ABI from
`dist/android/jniLibs/`. On Linux, build cps shared (`cmake -DCPS_BUILD_SHARED=ON`) and
link this example against it.
