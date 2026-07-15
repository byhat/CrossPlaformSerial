# third_party/usb-serial-for-android

This directory must contain the sources of the **usb-serial-for-android** library
(https://github.com/mik3y/usb-serial-for-android), used on Android for USB-serial
chipset drivers. The build compiles these `.java` sources (plus the cps JNI bridge
in `android/java/`) into a `classes.dex` that is embedded into `libcps.so` and
loaded at runtime via `DexClassLoader`.

## How to populate

Add it as a git submodule (recommended) or copy the sources in:

```
git submodule add https://github.com/mik3y/usb-serial-for-android.git \
  third_party/usb-serial-for-android
git submodule update --init --recursive
```

The CMake Android build globs `third_party/usb-serial-for-android/**/*.java`, so the
upstream layout (sources under `usbSerialForAndroid/src/main/java/`) works directly.

## Required on the classpath

The Java package expected by the cps bridge is `com.hoho.android.usbserial.*`
(the upstream mik3y fork). If you use a different fork, adjust the imports in
`android/java/com/cps/android/CpsUsbSerial.java`.

## License

See the upstream repository for its license terms; ensure compatibility with this
project's LGPL-2.1 before distribution.
