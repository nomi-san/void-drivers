/*
 * VoidInput - device-type registry.
 *
 * Each supported device type ships a fixed, compiled-in HID report descriptor
 * and a cloned identity (VID/PID/version). The driver looks the type up at
 * CREATE and hands the descriptor straight to VhfCreate - callers never send a
 * descriptor. Types not yet implemented return NULL.
 */

#pragma once

#include <windows.h>
#include "Public.h"

// Mouse report IDs - shared contract with the SDK packer (libvoidrv). The mouse
// is one device with two collections selected by report id.
#define VOIDINPUT_MOUSE_REPORT_RELATIVE 1
#define VOIDINPUT_MOUSE_REPORT_ABSOLUTE 2

// Keyboard report IDs - boot keyboard (modifiers + 6KRO) and consumer control.
#define VOIDINPUT_KBD_REPORT_KEYBOARD   1
#define VOIDINPUT_KBD_REPORT_CONSUMER   2

// Output/feature event kinds a type opts into (mapped to VHF callbacks when the
// device is created). The mouse is input-only and uses none; the bitmask has
// room for the gamepad/keyboard milestones.
#define VOIDINPUT_EVT_NONE         0x0u
#define VOIDINPUT_EVT_WRITE_REPORT 0x1u
#define VOIDINPUT_EVT_GET_FEATURE  0x2u
#define VOIDINPUT_EVT_SET_FEATURE  0x4u

typedef struct _VOIDINPUT_DEVICE_DESC {
    VOIDINPUT_DEVICE_TYPE Type;
    USHORT       DefaultVid;
    USHORT       DefaultPid;
    USHORT       DefaultVersion;
    const UCHAR* ReportDescriptor;
    USHORT       ReportDescriptorLength;
    BOOLEAN      NumberedReports;   // reports carry a leading report-id byte
    BOOLEAN      Singleton;         // mouse/keyboard/touch: at most one live
    BOOLEAN      IsGamepad;         // xbox/ds4/ds5: counts against the 4-pad cap
    ULONG        Events;            // VOIDINPUT_EVT_* the type handles (0 = input only)
} VOIDINPUT_DEVICE_DESC;

// Returns the compiled-in descriptor for a type, or NULL if not implemented.
const VOIDINPUT_DEVICE_DESC* VoidInputGetDeviceDesc(VOIDINPUT_DEVICE_TYPE type);
