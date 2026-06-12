/*
 * VoidInput - UMDF driver on the in-box Virtual HID Framework (VHF).
 *
 * One root-enumerated control device (Root\Void\Input) layered as an upper
 * filter on VHF. Each user-mode handle that issues IOCTL_VOIDINPUT_CREATE owns
 * one virtual HID device (created via VhfCreate); the device lives until the
 * handle is closed. The driver supplies the compiled-in HID report descriptor
 * for each device type - callers never send a descriptor. Input reports arrive
 * as WriteFile and are pushed to VHF; output/feature requests are surfaced to
 * the host through the GET_EVENT / COMPLETE_EVENT inverted-read channel.
 *
 * This is the enumerator + control-plane bring-up. The per-type HID descriptors
 * (mouse, keyboard, Xbox One, DS4, DS5, touch) land in the build-order
 * milestones that follow.
 */

#pragma once

#include <windows.h>
#include <wdf.h>
#include <vhf.h>

// Allocate GUID_DEVINTERFACE_VOIDINPUT in this single translation unit. Must
// precede Public.h, which declares it via DEFINE_GUID.
#include <initguid.h>
#include "Public.h"
#include "Trace.h"

EXTERN_C_START

#define VOIDINPUT_POOL_TAG 'nIoV'

// Per-handle context: one virtual HID device.
typedef struct _VOIDINPUT_FILE_CONTEXT {
    WDFFILEOBJECT FileObject;
    WDFIOTARGET   VhfIoTarget;       // the lower VHF device, opened by file
    VHF_CONFIG    VhfConfig;         // filled from the device type at CREATE
    VHFHANDLE     VhfHandle;         // non-null once the device is created
    VOIDINPUT_DEVICE_TYPE Type;      // VoidInputDeviceNone until created
    BOOLEAN       NumberedReports;   // the device's reports carry a report-id byte
} VOIDINPUT_FILE_CONTEXT, *PVOIDINPUT_FILE_CONTEXT;
WDF_DECLARE_CONTEXT_TYPE_WITH_NAME(VOIDINPUT_FILE_CONTEXT, VoidInputFileGetContext)

// Driver-wide device context: tracks the live device set for capacity + LIST.
typedef struct _VOIDINPUT_DEVICE_CONTEXT {
    WDFDEVICE   Device;
    WDFWAITLOCK Lock;                // guards the live-device table below
    UINT32      LiveCount;
    VOIDINPUT_ENTRY Live[VOIDINPUT_MAX_DEVICES];
} VOIDINPUT_DEVICE_CONTEXT, *PVOIDINPUT_DEVICE_CONTEXT;
WDF_DECLARE_CONTEXT_TYPE_WITH_NAME(VOIDINPUT_DEVICE_CONTEXT, VoidInputDeviceGetContext)

EXTERN_C_END

// Entry points and callbacks.
EXTERN_C DRIVER_INITIALIZE DriverEntry;

EVT_WDF_DRIVER_DEVICE_ADD          VoidInputDeviceAdd;
EVT_WDF_DEVICE_FILE_CREATE         VoidInputFileCreate;
EVT_WDF_FILE_CLOSE                 VoidInputFileClose;
EVT_WDF_IO_QUEUE_IO_DEVICE_CONTROL VoidInputIoDeviceControl;
EVT_WDF_IO_QUEUE_IO_WRITE          VoidInputIoWrite;
