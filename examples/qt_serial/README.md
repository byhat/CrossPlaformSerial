# cps Qt6 QML terminal

A cross-platform (Linux/Windows/macOS) serial terminal built with **Qt6 + QML** that
uses the cps C++ API. It demonstrates how to drive `cps::SerialPort`/`SerialPortInfo`
from Qt and marshal the library's worker-thread callbacks onto the GUI thread.

## Features

- Dropdown of all available ports (name + description/manufacturer/VID/PID).
- **Refresh** button to re-scan.
- **Connect/Disconnect** button (8N1, selectable baud).
- A scrollable read-only area that **accumulates all incoming bytes**.
- **Clear** button (also flushes the port buffers).
- **Send** button that transmits the text typed in the field above it (UTF-8).

## How it works

- `PortListModel` (QAbstractListModel) wraps `cps::SerialPortInfo::availablePorts()`
  and exposes portName/description/manufacturer/vid/pid roles to the QML ComboBox.
- `SerialController` (QObject) owns a `cps::SerialPort`, exposes `connectPort()`,
  `disconnectPort()`, `send()`, `clear()` to QML, and signals `dataReceived(QString)`
  and `errorOccurred(QString)`.
- The cps `onReadyRead`/`onError` callbacks fire on the library's **worker thread**;
  the controller reads the bytes there and emits signals, which Qt auto-queues to the
  GUI thread — QML is never touched off-thread. Fatal errors are closed on the GUI
  thread (closing joins the worker, so it can't run on the worker itself).
- Received bytes are shown via `QString::fromLatin1` (1 byte → 1 code point, no data
  loss for binary). Change to `fromUtf8` if you prefer decoded text.

## Build

Build cps first (so the `cps` target exists), then point CMake at your Qt6 install:

```sh
# 1. cps (shared or static)
cmake -S . -B build -DCMAKE_PREFIX_PATH=/path/to/Qt/6.x.y/compiler
cmake --build build

# 2. the example is built automatically when Qt6 is found
```

On Windows (Qt 6.9 / MinGW 13.1 example):

```powershell
cmake -S . -B build-qt -G "MinGW Makefiles" `
    -DCMAKE_PREFIX_PATH=C:/Qt/6.9.0/mingw_64
cmake --build build-qt --target cps_qt_serial
```

The example is only added when `Qt6` (>= 6.2, `Core`/`Gui`/`Quick`) is found, so
non-Qt builds are unaffected.

## Run

The executable needs the cps shared library and the Qt runtime on its path.

```powershell
# Windows: prepend Qt's bin + the build dir (for libcps.dll)
$env:Path = "C:\Qt\6.9.0\mingw_64\bin;.\build-qt;$env:Path"
.\build-qt\examples\qt_serial\cps_qt_serial.exe
```

```sh
# Linux: build dir already has libcps.so; Qt libs come from the system/your install
./build-qt/examples/qt_serial/cps_qt_serial
```

## Notes

- Sent text is not echoed into the receive area (it shows incoming bytes only).
- To see traffic, point the combo at a real port (e.g. a USB-serial adapter) and set
  the matching baud rate.

## Running on Android

The same example builds for **Android** with Qt for Android (it then links the cps JNI
backend + embedded `usb-serial-for-android` dex instead of the asio backend).

- **Initialization is handled for you:** `main.cpp` calls
  `cps_serial_init_android(javaVM, androidContext)` (via `QJniEnvironment::javaVM()` and
  `QNativeInterface::QAndroidApplication::context()`) before the first enumeration. This
  loads the embedded dex, registers the native callbacks and hands the `UsbManager`
  context to the bridge — **without this call, Refresh lists nothing.**
- **Permission:** cps requests USB permission at `Connect` time (runtime dialog), so no
  manifest entry is required for enumeration or connection.
- **Optional — auto-permission on plug-in:** drop `android/res/xml/device_filter.xml`
  into your package source dir and add to `AndroidManifest.xml`:

  ```xml
  <!-- under <manifest>: -->
  <uses-feature android:name="android.hardware.usb.host" android:required="false" />

  <!-- inside the launcher <activity>, next to the existing MAIN/LAUNCHER filter: -->
  <intent-filter>
      <action android:name="android.hardware.usb.action.USB_DEVICE" />
  </intent-filter>
  <meta-data
      android:name="android.hardware.usb.action.USB_DEVICE"
      android:resource="@xml/device_filter" />
  ```

  Point androiddeployqt at the dir via the target property
  `QT_ANDROID_PACKAGE_SOURCE_DIR`. This makes Android pop the "open with cps_qt_serial"
  chooser + permission dialog when the adapter is plugged in.

