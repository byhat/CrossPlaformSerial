/**
 * @file cps.h
 * @brief Stable C ABI for the cps cross-platform serial library.
 *
 * Pure C, dependency-free (only <stdint.h>/<stddef.h>). This is the portable surface
 * usable from any language. The C++ API in <cps/*.hpp> is also available but its ABI
 * across a shared-library boundary requires a compatible compiler/STL; prefer this C
 * ABI for cross-toolchain consumers.
 *
 * Strings in cps_PortInfo are UTF-8, NUL-terminated, owned by the library and valid
 * only until cps_free_ports() is called. Copy them if you need them longer.
 */
#ifndef CPS_H
#define CPS_H

#include <cps/cps_export.h>
#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ---- types --------------------------------------------------------------- */

/** Opaque handle to a serial port instance. */
typedef struct cps_SerialPort cps_SerialPort;

/** Information about an available port (mirrors cps::SerialPortInfo). */
typedef struct {
    const char *   port_name;
    const char *   description;
    const char *   manufacturer;
    const char *   serial_number;
    uint16_t       product_id;   /* PID */
    uint16_t       vendor_id;    /* VID */
    const char *   system_location;
} cps_PortInfo;

/* C enums mirror cps:: enum class integer values (locked by static_assert). */
typedef enum {
    CPS_DATA_5 = 5, CPS_DATA_6 = 6, CPS_DATA_7 = 7, CPS_DATA_8 = 8
} cps_DataBits;

typedef enum {
    CPS_PAR_NONE  = 0, CPS_PAR_EVEN = 1, CPS_PAR_ODD = 2,
    CPS_PAR_SPACE = 3, CPS_PAR_MARK = 4
} cps_Parity;

typedef enum {
    CPS_STOP_ONE = 0, CPS_STOP_ONE_HALF = 1, CPS_STOP_TWO = 2
} cps_StopBits;

typedef enum {
    CPS_FLOW_NONE = 0, CPS_FLOW_HARDWARE = 1, CPS_FLOW_SOFTWARE = 2
} cps_FlowControl;

typedef enum {
    CPS_DIR_INPUT = 1, CPS_DIR_OUTPUT = 2, CPS_DIR_ALL = 3
} cps_Directions;

typedef enum {
    CPS_MODE_READ = 1, CPS_MODE_WRITE = 2, CPS_MODE_READWRITE = 3
} cps_OpenMode;

typedef enum {
    CPS_ERR_NONE = 0,
    CPS_ERR_DEVICE_NOT_FOUND,
    CPS_ERR_PERMISSION,
    CPS_ERR_OPEN,
    CPS_ERR_PARITY,
    CPS_ERR_FRAMING,
    CPS_ERR_WRITE,
    CPS_ERR_READ,
    CPS_ERR_RESOURCE,
    CPS_ERR_UNSUPPORTED_OPERATION,
    CPS_ERR_UNKNOWN,
    CPS_ERR_TIMEOUT,
    CPS_ERR_NOT_OPEN
} cps_SerialError;

/* ---- enumeration --------------------------------------------------------- */

/** Returns a heap array of port infos; writes count to *out_count (may be NULL).
 *  Free with cps_free_ports(). Returns NULL if there are no ports. */
CPS_API cps_PortInfo * cps_available_ports(int *out_count);
CPS_API void           cps_free_ports(cps_PortInfo *ports);

/* ---- lifecycle ----------------------------------------------------------- */

CPS_API cps_SerialPort * cps_serial_new(const char *port_name /*nullable*/);
CPS_API void             cps_serial_free(cps_SerialPort *port);

/* ---- config + io --------------------------------------------------------- */
/* setters return cps_SerialError; read/write return byte count or -1 (see cps_serial_error). */

CPS_API cps_SerialError cps_serial_set_port_name(cps_SerialPort *port, const char *name);
CPS_API cps_SerialError cps_serial_open(cps_SerialPort *port, cps_OpenMode mode);
CPS_API void            cps_serial_close(cps_SerialPort *port);
CPS_API int             cps_serial_is_open(cps_SerialPort *port);

CPS_API long cps_serial_write(cps_SerialPort *port, const void *data, long size);
CPS_API long cps_serial_read(cps_SerialPort *port, void *buf, long max_size);

CPS_API cps_SerialError cps_serial_set_baud_rate(cps_SerialPort *port, int rate, cps_Directions dir);
CPS_API cps_SerialError cps_serial_set_data_bits(cps_SerialPort *port, cps_DataBits bits);
CPS_API cps_SerialError cps_serial_set_parity(cps_SerialPort *port, cps_Parity parity);
CPS_API cps_SerialError cps_serial_set_stop_bits(cps_SerialPort *port, cps_StopBits bits);
CPS_API cps_SerialError cps_serial_set_flow_control(cps_SerialPort *port, cps_FlowControl flow);

CPS_API long            cps_serial_bytes_available(cps_SerialPort *port);
CPS_API long            cps_serial_bytes_to_write(cps_SerialPort *port);
CPS_API cps_SerialError cps_serial_flush(cps_SerialPort *port);
CPS_API cps_SerialError cps_serial_clear(cps_SerialPort *port, cps_Directions dir);

CPS_API cps_SerialError cps_serial_error(cps_SerialPort *port);
CPS_API void            cps_serial_clear_error(cps_SerialPort *port);

/* ---- callbacks ----------------------------------------------------------- */

typedef void (*cps_ready_read_fn)(void *user_data);
typedef void (*cps_bytes_written_fn)(void *user_data, long bytes);
typedef void (*cps_error_fn)(void *user_data, cps_SerialError err);

typedef struct {
    cps_ready_read_fn    on_ready_read;
    cps_bytes_written_fn on_bytes_written;
    cps_error_fn         on_error;
} cps_Callbacks;

CPS_API void cps_serial_set_callbacks(cps_SerialPort *port,
                                      const cps_Callbacks *callbacks,
                                      void *user_data);

/* ---- android init -------------------------------------------------------- */
/* No-op returning CPS_ERR_UNSUPPORTED_OPERATION on desktop.
 * On Android pass the JavaVM* (or NULL to use the one captured in JNI_OnLoad) and the
 * application Context jobject. Must be called once before any port is opened. */
CPS_API cps_SerialError cps_serial_init_android(void *java_vm /*nullable, JavaVM**/,
                                                void *android_context /*jobject*/);

/* ---- misc ---------------------------------------------------------------- */

CPS_API const char *   cps_version(void);
CPS_API const char *   cps_error_string(cps_SerialError err);

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* CPS_H */
