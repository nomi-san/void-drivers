/*
 * VoidInput - shared control interface.
 *
 * This header is the contract between the driver and user-mode callers
 * (libvoidrv / voidctl). It is plain C and may be included from either side.
 * The includer is expected to have pulled in <windows.h> first.
 *
 * Model: one virtual HID device per open handle. A caller opens the control
 * interface, issues IOCTL_VOIDINPUT_CREATE to make a typed device on that
 * handle, then streams input reports with WriteFile (the hot path) and services
 * output/feature requests through IOCTL_VOIDINPUT_GET_EVENT. Closing the handle
 * removes the device.
 */

#pragma once

#include <winioctl.h>
#include <guiddef.h>

/* Control device interface exposed by VoidInput. */
/* {7b0c8d49-54fc-45ca-ab47-6a572f2ff510} */
DEFINE_GUID(GUID_DEVINTERFACE_VOIDINPUT,
    0x7b0c8d49, 0x54fc, 0x45ca, 0xab, 0x47, 0x6a, 0x57, 0x2f, 0x2f, 0xf5, 0x10);

#define VOIDINPUT_VERSION       1
#define VOIDINPUT_MAX_DEVICES   8
#define VOIDINPUT_MAX_REPORT    256   /* largest report payload we carry either way */

/* Device types. The driver supplies the compiled-in HID descriptor per type;
   callers never send a descriptor. */
typedef enum _VOIDINPUT_DEVICE_TYPE {
    VoidInputDeviceNone     = 0,
    VoidInputDeviceMouse    = 1,   /* relative + absolute, singleton */
    VoidInputDeviceKeyboard = 2,   /* boot + consumer + system control, singleton */
    VoidInputDeviceXboxOne  = 3,   /* HID game pad */
    VoidInputDeviceDS4      = 4,   /* DualShock 4 */
    VoidInputDeviceDS5      = 5,   /* DualSense (input-only) */
    VoidInputDeviceTouch    = 6,   /* precision multi-touch digitizer, singleton */
} VOIDINPUT_DEVICE_TYPE;

#define VOIDINPUT_IOCTL(i, access) \
    CTL_CODE(FILE_DEVICE_UNKNOWN, 0x800 + (i), METHOD_BUFFERED, (access))

/* Query interface/driver version. Out: ULONG (VOIDINPUT_VERSION). */
#define IOCTL_VOIDINPUT_VERSION        VOIDINPUT_IOCTL(1, FILE_READ_ACCESS | FILE_WRITE_ACCESS)

/* Create the virtual HID device on this handle. In: VOIDINPUT_CREATE. */
#define IOCTL_VOIDINPUT_CREATE         VOIDINPUT_IOCTL(2, FILE_WRITE_ACCESS)

/* Destroy the device on this handle (also implicit on handle close). */
#define IOCTL_VOIDINPUT_DESTROY        VOIDINPUT_IOCTL(3, FILE_WRITE_ACCESS)

/* List live devices across the driver (diagnostics). Out: VOIDINPUT_STATE. */
#define IOCTL_VOIDINPUT_LIST           VOIDINPUT_IOCTL(4, FILE_READ_ACCESS | FILE_WRITE_ACCESS)

/* Pend for the next output/feature event for this device. Out: VOIDINPUT_EVENT. */
#define IOCTL_VOIDINPUT_GET_EVENT      VOIDINPUT_IOCTL(5, FILE_READ_ACCESS | FILE_WRITE_ACCESS)

/* Complete an output/feature event previously returned. In: VOIDINPUT_EVENT_COMPLETE. */
#define IOCTL_VOIDINPUT_COMPLETE_EVENT VOIDINPUT_IOCTL(6, FILE_WRITE_ACCESS)

#include <pshpack1.h>

typedef struct _VOIDINPUT_CREATE {
    UINT32 Type;            /* VOIDINPUT_DEVICE_TYPE */
    UINT32 Flags;           /* reserved, must be 0 */
    UINT16 VendorId;        /* 0 = use the type's default cloned VID */
    UINT16 ProductId;       /* 0 = use the type's default cloned PID */
    UINT16 VersionNumber;   /* 0 = type default */
    UINT16 Reserved;
} VOIDINPUT_CREATE;

typedef struct _VOIDINPUT_ENTRY {
    UINT32 Type;            /* VOIDINPUT_DEVICE_TYPE, 0 if the slot is empty */
    UINT16 VendorId;
    UINT16 ProductId;
} VOIDINPUT_ENTRY;

typedef struct _VOIDINPUT_STATE {
    UINT32 Count;           /* number of live devices */
    VOIDINPUT_ENTRY Entries[VOIDINPUT_MAX_DEVICES];
} VOIDINPUT_STATE;

/* Output/feature request kinds handed to the host (device -> host). */
typedef enum _VOIDINPUT_EVENT_TYPE {
    VoidInputEventNone        = 0,
    VoidInputEventWriteReport = 1,  /* output report: rumble, LED, lightbar */
    VoidInputEventSetFeature  = 2,  /* feature write */
    VoidInputEventGetFeature  = 3,  /* feature read (host must supply data on complete) */
} VOIDINPUT_EVENT_TYPE;

typedef struct _VOIDINPUT_EVENT {
    UINT32 Type;            /* VOIDINPUT_EVENT_TYPE */
    UINT32 RequestId;       /* opaque; echo back in VOIDINPUT_EVENT_COMPLETE */
    UINT8  ReportId;
    UINT8  Reserved[3];
    UINT32 DataLength;      /* bytes valid in Data */
    UINT8  Data[VOIDINPUT_MAX_REPORT];
} VOIDINPUT_EVENT;

typedef struct _VOIDINPUT_EVENT_COMPLETE {
    UINT32 RequestId;
    INT32  Status;          /* 0 = success */
    UINT32 DataLength;      /* GetFeature: bytes the host is returning */
    UINT8  Data[VOIDINPUT_MAX_REPORT];
} VOIDINPUT_EVENT_COMPLETE;

#include <poppack.h>
