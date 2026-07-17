# cps on Android — build & integration

cps on Android is backed by the vendored **usb-serial-for-android** library, which is
compiled to a `classes.dex` and **embedded into `libcps.so`** (loaded at runtime via a
`DexClassLoader`). The consumer never compiles or links Java from cps.

This guide covers: prerequisites, building `libcps.so` for every ABI, and wiring it into
an Android app.

## 1. Prerequisites

| Tool | Needed for | Notes |
|------|------------|-------|
| Android NDK | compiling the `.so` | set `ANDROID_NDK_ROOT` (or `ANDROID_NDK`) |
| Android SDK | `android.jar` + `d8` (dex) | set `ANDROID_SDK_ROOT`; install a **platform** (`platforms/android-34`+) and **build-tools** |
| JDK (javac) | compiling java → dex | JDK 11+; `javac` on `PATH` or `JAVA_HOME` set |
| CMake 3.16+, a make tool | the build | Ninja or Make/MinGW |

The `usb-serial-for-android` sources must be present (git submodule):

```sh
git submodule update --init --recursive
```

## 2. Build all ABIs

A helper script configures a **separate build directory per ABI** and lays the results
out as `jniLibs/<abi>/libcps.so`:

```sh
# Linux / macOS
scripts/build-android.sh

# Windows (PowerShell)
scripts\build-android.ps1
```

Options (env vars on the shell script, params on PowerShell):

- **ABIs** — default `arm64-v8a armeabi-v7a x86 x86_64`. Pass a subset:
  `scripts/build-android.sh arm64-v8a armeabi-v7a` / `.\build-android.ps1 -Abis arm64-v8a`.
- **Min API** — `MIN_API=android-21` / `-MinApi android-21` (default `android-21`, must
  match `CPS_ANDROID_MIN_API`).
- **Output** — `OUT_DIR=dist/android` / `-OutDir` (default `dist/android`).
- **Clean** — `CLEAN=1` / `-Clean`.

Output layout:

```
dist/android/
  jniLibs/
    arm64-v8a/libcps.so
    armeabi-v7a/libcps.so
    x86/libcps.so
    x86_64/libcps.so
  include/cps/        # cps.h (C ABI) + the C++ headers
```

> Each `libcps.so` already contains the embedded `classes.dex`; there is **no separate
> dex to ship**.

## 3. Consume in an Android app

### 3a. Bundle the native libraries

Copy the `jniLibs/<abi>/` folders into your Gradle app module so they end up packaged in
the APK:

```
app/src/main/jniLibs/
  arm64-v8a/libcps.so
  armeabi-v7a/libcps.so
  x86/libcps.so
  x86_64/libcps.so
```

Either copy them manually, or with a one-liner from the repo root:

```sh
cp -r dist/android/jniLibs/* <your-app>/src/main/jniLibs/
```

### 3b. Load + initialize once

In your `Activity`/`Application` (Kotlin/Java), load the library and pass the Android
`Context` to cps before first use:

```kotlin
class MyApp : Application() {
    override fun onCreate() {
        super.onCreate()
        System.loadLibrary("cps")                  // triggers JNI_OnLoad
        cpsInitAndroid(this)                        // pass the Context
    }

    private external fun cpsInitAndroid(ctx: Any)   // -> cps_serial_init_android(NULL, ctx)
    companion object { init { /* native decls */ } }
}
```

From C/C++ (NDK side) that call is:

```c
#include <cps/cps.h>
cps_serial_init_android(/*java_vm=*/NULL, /*android_context=*/context_jobject);
```

`cps_serial_init_android` captures the `Context` (needed to access the `UsbManager`).
Pass `NULL` for `java_vm` to use the one cps already captured in `JNI_OnLoad`.

### 3c. Use the C ABI

Once initialized, open ports by the device name returned by enumeration (see
`examples/android_snippet.cpp` and `examples/c_minimal.c`):

```c
cps_SerialPort* p = cps_serial_new(device_name);
cps_serial_set_baud_rate(p, 9600, CPS_DIR_ALL);
cps_Callbacks cb = { .on_ready_read = my_ready_read };
cps_serial_set_callbacks(p, &cb, NULL);
cps_serial_open(p, CPS_MODE_READWRITE);
cps_serial_write(p, "hi", 2);
```

## 4. Notes

- **Threading:** user callbacks (`on_ready_read`, `on_error`) fire on the library's
  internal worker thread — marshal to the UI thread if needed.
- **USB permission:** cps auto-requests USB permission on open (broadcast-based). The
  user must grant it.
- **ABI filter:** ship only the ABIs you need (usually `arm64-v8a` and `armeabi-v7a`) to
  keep the APK small; `x86`/`x86_64` are useful for the emulator.
- **Vendored library version:** the build uses whatever `third_party/usb-serial-for-android`
  the submodule points at; the bridge in `android/java/` is kept in sync with that API.
