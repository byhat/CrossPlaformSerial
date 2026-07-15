/* Minimal C consumer exercising the C ABI.
 * Usage: ./c_minimal [/dev/ttyUSB0]   (then press Enter to quit) */
#include <cps/cps.h>

#include <stdio.h>
#include <string.h>

static void on_ready_read(void* user) {
    cps_SerialPort* p = (cps_SerialPort*)user;
    unsigned char buf[256];
    long n = cps_serial_read(p, buf, sizeof(buf));
    if (n > 0) {
        printf("received %ld byte(s):", n);
        for (long i = 0; i < n; ++i) printf(" %02X", buf[i]);
        printf("\n");
    }
}

int main(int argc, char** argv) {
    const char* name = (argc > 1) ? argv[1] : "/dev/ttyUSB0";

    cps_SerialPort* p = cps_serial_new(name);
    cps_serial_set_baud_rate(p, 9600, CPS_DIR_ALL);
    cps_serial_set_data_bits(p, CPS_DATA_8);
    cps_serial_set_parity(p, CPS_PAR_NONE);
    cps_serial_set_stop_bits(p, CPS_STOP_ONE);

    cps_Callbacks cb;
    cb.on_ready_read = &on_ready_read;
    cb.on_bytes_written = NULL;
    cb.on_error = NULL;
    cps_serial_set_callbacks(p, &cb, p);

    if (cps_serial_open(p, CPS_MODE_READWRITE) != CPS_ERR_NONE) {
        printf("Failed to open %s: %s\n", name, cps_error_string(cps_serial_error(p)));
        cps_serial_free(p);
        return 1;
    }

    const char* hello = "hello\n";
    cps_serial_write(p, hello, (long)strlen(hello));

    printf("Opened %s (cps %s). Press Enter to quit.\n", name, cps_version());
    getchar();

    cps_serial_close(p);
    cps_serial_free(p);
    return 0;
}
