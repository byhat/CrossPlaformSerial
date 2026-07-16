# cps Go example (cgo)

Consumes `libcps` through the C ABI using cgo. By default it:

1. scans available ports for one with **vendor id 0x1a86** (CH340/CH341 USB-serial),
2. opens it (9600 8N1),
3. writes **`hello serial`**,
4. prints any incoming data, received through a real **C -> Go callback** for
   `readyRead` (the library's worker thread calls back into Go via an `//export`
   function).

The cgo directives resolve the headers and the library via paths relative to this
directory, so no install step is required. They are split per platform:

```c
#cgo CFLAGS:  -I${SRCDIR}/../../include
#cgo !windows LDFLAGS: -L${SRCDIR}/../../build -lcps -Wl,-rpath,${SRCDIR}/../../build
#cgo windows  LDFLAGS: -L${SRCDIR}/../../build -lcps -lstdc++ -lwinpthread -lws2_32 -lsetupapi
```

- On **Linux/macOS** the shared `libcps.so` is linked and resolved at runtime via
  `-rpath`, so no `LD_LIBRARY_PATH` is needed.
- On **Windows** the static `libcps.a` is linked into the executable (no `cps.dll`
  needed at runtime). `-lstdc++ -lwinpthread` supply the C++ runtime that the static
  archive references; the MinGW runtime DLLs on `PATH` provide them at run time.

> **Important (Windows):** the C/MinGW toolchain, the Boost build and Go must all be
> the **same architecture**. A 32-bit Go (`GOARCH=386`) cannot link the 64-bit
> `libcps.a`. Install the **64-bit** Go (`GOARCH=amd64`) to match a 64-bit build.

## Build the library first

From the repo root:

```sh
# Linux / macOS  -> shared lib + rpath
cmake -S . -B build -DCPS_BUILD_SHARED=ON
cmake --build build

# Windows        -> static lib (recommended for `go run`)
cmake -S . -B build -DCPS_BUILD_SHARED=OFF
cmake --build build
```

On Windows, make sure the MinGW bin dir (e.g. `C:\Qt\Tools\mingw1310_64\bin`) is on
`PATH` so cgo finds `gcc` and the runtime DLLs.

## Run

From this directory:

```sh
go run .               # auto-find the CH340 port and talk to it
go run . /dev/ttyUSB0  # (Linux/test) open a specific port path directly
go run . COM14         # (Windows)     open a named port directly
```
