// cps C-ABI consumer in Go (cgo). Links libcps and drives it through the C API,
// including a real C -> Go callback (readyRead) invoked from the library's worker
// thread.
//
// Default behaviour: scan available ports for one with vendor id 0x1a86
// (CH340/CH341 USB-serial), open it, write "hello serial", and print any incoming data.
//
// Build the library first (from the repo root):
//
//	Linux/macOS:  cmake -S . -B build -DCPS_BUILD_SHARED=ON && cmake --build build
//	Windows:      cmake -S . -B build -DCPS_BUILD_SHARED=OFF && cmake --build build
//
// (On Windows a static libcps.a avoids DLL-search issues when running via `go run`;
//
//	on Linux the shared libcps.so is resolved at runtime through -rpath.)
//
// Then, from this directory:
//
//	go run .                       # auto-find the CH340 port
//	go run . /tmp/cpsA             # (test) open a specific port path directly
//	go run . COM3                  # open a named port directly
package main

/*
#cgo CFLAGS:  -I${SRCDIR}/../../include
#cgo !windows LDFLAGS: -L${SRCDIR}/../../build -lcps -Wl,-rpath,${SRCDIR}/../../build
#cgo windows  LDFLAGS: -L${SRCDIR}/../../build -lcps -lstdc++ -lwinpthread -lws2_32 -lsetupapi

#include <cps/cps.h>
#include <stdlib.h>

// Implemented in Go (see //export cps_go_ready_read below); declared here so the C
// helper can install it as the readyRead callback.
extern void cps_go_ready_read(void *user);

// Convenience: build a cps_Callbacks pointing at the Go callback and install it.
static void cps_go_install_callbacks(cps_SerialPort *port) {
    cps_Callbacks cb;
    cb.on_ready_read    = cps_go_ready_read;
    cb.on_bytes_written = NULL;
    cb.on_error         = NULL;
    cps_serial_set_callbacks(port, &cb, NULL);
}
*/
import "C"

import (
	"fmt"
	"os"
	"strings"
	"time"
	"unsafe"
)

const targetVID = 0x1a86 // CH340 LoRa USB

// readyCh is signalled from the C->Go callback (which runs on the library's worker
// thread). It does not receive a Go pointer from C, so it is cgo-pointer-rule safe.
var readyCh = make(chan struct{}, 1)

//export cps_go_ready_read
func cps_go_ready_read(_ unsafe.Pointer) {
	select {
	case readyCh <- struct{}{}:
	default: // coalesce; we drain on the next read
	}
}

// findPortByVID scans available ports and returns the name of the first one whose
// vendor id matches, or "" if none.
func findPortByVID(vid uint16) (string, uint16, bool) {
	var count C.int
	infos := C.cps_available_ports(&count)
	defer C.cps_free_ports(infos)
	if count == 0 {
		return "", 0, false
	}
	slice := unsafe.Slice(infos, int(count))
	for _, p := range slice {
		if uint16(p.vendor_id) == vid {
			return C.GoString(p.port_name), uint16(p.product_id), true
		}
	}
	return "", 0, false
}

func listPorts() {
	var count C.int
	infos := C.cps_available_ports(&count)
	defer C.cps_free_ports(infos)
	if count == 0 {
		fmt.Println("No serial ports found.")
		return
	}
	slice := unsafe.Slice(infos, int(count))
	fmt.Printf("Found %d port(s):\n", count)
	for _, p := range slice {
		fmt.Printf("  %-16s vid=0x%04x pid=0x%04x  %s\n",
			C.GoString(p.port_name), p.vendor_id, p.product_id, C.GoString(p.description))
	}
}

func main() {
	fmt.Println("cps", C.GoString(C.cps_version()))

	portName := ""
	if len(os.Args) > 1 {
		arg := os.Args[1]
		if strings.HasPrefix(arg, "/") || strings.HasPrefix(strings.ToUpper(arg), "COM") {
			// Direct port path ("/dev/ttyUSB0" on Unix, "COM3" on Windows).
			portName = arg
			fmt.Printf("Using port from argument: %s\n", portName)
		}
	}

	if portName == "" {
		name, pid, ok := findPortByVID(targetVID)
		if !ok {
			fmt.Printf("No serial port with vendor id %d (0x%04x) found.\n", targetVID, targetVID)
			listPorts()
			os.Exit(1)
		}
		fmt.Printf("Found VID %d device: %s (pid=0x%04x)\n", targetVID, name, pid)
		portName = name
	}

	cname := C.CString(portName)
	port := C.cps_serial_new(cname)
	C.free(unsafe.Pointer(cname))
	if port == nil {
		fmt.Fprintln(os.Stderr, "cps_serial_new failed")
		os.Exit(1)
	}
	defer C.cps_serial_free(port)

	C.cps_serial_set_baud_rate(port, 9600, C.CPS_DIR_ALL)
	C.cps_serial_set_data_bits(port, C.CPS_DATA_8)
	C.cps_serial_set_parity(port, C.CPS_PAR_NONE)
	C.cps_serial_set_stop_bits(port, C.CPS_STOP_ONE)
	C.cps_go_install_callbacks(port)

	if C.cps_serial_open(port, C.CPS_MODE_READWRITE) != C.CPS_ERR_NONE {
		fmt.Fprintln(os.Stderr, "open failed:",
			C.GoString(C.cps_error_string(C.cps_serial_error(port))))
		os.Exit(1)
	}
	defer C.cps_serial_close(port)
	fmt.Printf("Opened %s.\n", portName)

	msg := []byte("hello serial")
	if n := C.cps_serial_write(port, unsafe.Pointer(&msg[0]), C.long(len(msg))); n < 0 {
		fmt.Fprintln(os.Stderr, "write failed")
		os.Exit(1)
	}
	fmt.Printf("Wrote %d byte(s): %q\n", len(msg), string(msg))
	fmt.Println("Listening for incoming data (Ctrl-C to quit)...")

	buf := make([]byte, 512)
	for {
		select {
		case <-readyCh:
			for {
				n := C.cps_serial_bytes_available(port)
				if n <= 0 {
					break
				}
				r := C.cps_serial_read(port, unsafe.Pointer(&buf[0]), C.long(len(buf)))
				if r <= 0 {
					break
				}
				fmt.Printf("recv %d byte(s): %q\n", r, string(buf[:r]))
			}
		case <-time.After(5 * time.Second):
			// idle tick (keeps the loop alive while waiting)
		}
	}
}
