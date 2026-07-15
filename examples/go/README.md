# cps Go example (cgo)

Consumes `libcps.so` through the C ABI using cgo. By default it:

1. scans available ports for one with **vendor id 1155** (0x0483, STMicroelectronics),
2. opens it (115200 8N1),
3. writes **`hello serial`**,
4. prints any incoming data, received through a real **C -> Go callback** for
   `readyRead` (the library's worker thread calls back into Go via an `//export`
   function).

The cgo directives resolve the headers and the shared library via paths relative to
this directory, so no install step is required:

```c
#cgo CFLAGS:  -I${SRCDIR}/../../include
#cgo LDFLAGS: -L${SRCDIR}/../../build -lcps -Wl,-rpath,${SRCDIR}/../../build
```

## Build the .so first

From the repo root:

```sh
cmake -S . -B build -DCPS_BUILD_SHARED=ON
cmake --build build
```

## Run

From this directory:

```sh
go run .            # auto-find the VID 1155 port and talk to it
go run . /tmp/cpsA  # (test) open a specific port path directly
```

Because the runtime finds `libcps.so` via `-rpath`, you do not need to set
`LD_LIBRARY_PATH`.
