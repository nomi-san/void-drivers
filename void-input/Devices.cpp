#include "Devices.h"

// ---------------------------------------------------------------------------
// Mouse
//
// One HID device exposing two collections, selected by report id:
//   ID 1 - relative pointer: signed 16-bit X/Y deltas
//   ID 2 - absolute pointer: unsigned 16-bit X/Y over a 0..32767 range
// Both carry a 5-button bitmask (L=0x01 R=0x02 M=0x04 X1=0x08 X2=0x10), a
// vertical wheel, and a horizontal wheel (AC Pan). Report bytes after the
// report-id byte:  buttons(1) + X(2) + Y(2) + wheel(1) + hwheel(1) = 7,
// so each report is 8 bytes including the id.
// ---------------------------------------------------------------------------
static const UCHAR k_MouseReportDescriptor[] = {
    // ---- Relative pointer (Report ID 1) ----
    0x05, 0x01,        // Usage Page (Generic Desktop)
    0x09, 0x02,        // Usage (Mouse)
    0xA1, 0x01,        // Collection (Application)
    0x85, 0x01,        //   Report ID (1)
    0x09, 0x01,        //   Usage (Pointer)
    0xA1, 0x00,        //   Collection (Physical)
    0x05, 0x09,        //     Usage Page (Button)
    0x19, 0x01,        //     Usage Minimum (Button 1)
    0x29, 0x05,        //     Usage Maximum (Button 5)
    0x15, 0x00,        //     Logical Minimum (0)
    0x25, 0x01,        //     Logical Maximum (1)
    0x95, 0x05,        //     Report Count (5)
    0x75, 0x01,        //     Report Size (1)
    0x81, 0x02,        //     Input (Data,Var,Abs)        ; 5 buttons
    0x95, 0x01,        //     Report Count (1)
    0x75, 0x03,        //     Report Size (3)
    0x81, 0x03,        //     Input (Const,Var,Abs)       ; 3-bit pad
    0x05, 0x01,        //     Usage Page (Generic Desktop)
    0x09, 0x30,        //     Usage (X)
    0x09, 0x31,        //     Usage (Y)
    0x16, 0x01, 0x80,  //     Logical Minimum (-32767)
    0x26, 0xFF, 0x7F,  //     Logical Maximum (32767)
    0x75, 0x10,        //     Report Size (16)
    0x95, 0x02,        //     Report Count (2)
    0x81, 0x06,        //     Input (Data,Var,Rel)        ; X, Y relative
    0x09, 0x38,        //     Usage (Wheel)
    0x15, 0x81,        //     Logical Minimum (-127)
    0x25, 0x7F,        //     Logical Maximum (127)
    0x75, 0x08,        //     Report Size (8)
    0x95, 0x01,        //     Report Count (1)
    0x81, 0x06,        //     Input (Data,Var,Rel)        ; wheel
    0x05, 0x0C,        //     Usage Page (Consumer)
    0x0A, 0x38, 0x02,  //     Usage (AC Pan)
    0x15, 0x81,        //     Logical Minimum (-127)
    0x25, 0x7F,        //     Logical Maximum (127)
    0x75, 0x08,        //     Report Size (8)
    0x95, 0x01,        //     Report Count (1)
    0x81, 0x06,        //     Input (Data,Var,Rel)        ; horizontal wheel
    0xC0,              //   End Collection (Physical)
    0xC0,              // End Collection (Application)

    // ---- Absolute pointer (Report ID 2) ----
    0x05, 0x01,        // Usage Page (Generic Desktop)
    0x09, 0x02,        // Usage (Mouse)
    0xA1, 0x01,        // Collection (Application)
    0x85, 0x02,        //   Report ID (2)
    0x09, 0x01,        //   Usage (Pointer)
    0xA1, 0x00,        //   Collection (Physical)
    0x05, 0x09,        //     Usage Page (Button)
    0x19, 0x01,        //     Usage Minimum (Button 1)
    0x29, 0x05,        //     Usage Maximum (Button 5)
    0x15, 0x00,        //     Logical Minimum (0)
    0x25, 0x01,        //     Logical Maximum (1)
    0x95, 0x05,        //     Report Count (5)
    0x75, 0x01,        //     Report Size (1)
    0x81, 0x02,        //     Input (Data,Var,Abs)        ; 5 buttons
    0x95, 0x01,        //     Report Count (1)
    0x75, 0x03,        //     Report Size (3)
    0x81, 0x03,        //     Input (Const,Var,Abs)       ; 3-bit pad
    0x05, 0x01,        //     Usage Page (Generic Desktop)
    0x09, 0x30,        //     Usage (X)
    0x09, 0x31,        //     Usage (Y)
    0x15, 0x00,        //     Logical Minimum (0)
    0x26, 0xFF, 0x7F,  //     Logical Maximum (32767)
    0x75, 0x10,        //     Report Size (16)
    0x95, 0x02,        //     Report Count (2)
    0x81, 0x02,        //     Input (Data,Var,Abs)        ; X, Y absolute
    0x09, 0x38,        //     Usage (Wheel)
    0x15, 0x81,        //     Logical Minimum (-127)
    0x25, 0x7F,        //     Logical Maximum (127)
    0x75, 0x08,        //     Report Size (8)
    0x95, 0x01,        //     Report Count (1)
    0x81, 0x06,        //     Input (Data,Var,Rel)        ; wheel
    0x05, 0x0C,        //     Usage Page (Consumer)
    0x0A, 0x38, 0x02,  //     Usage (AC Pan)
    0x15, 0x81,        //     Logical Minimum (-127)
    0x25, 0x7F,        //     Logical Maximum (127)
    0x75, 0x08,        //     Report Size (8)
    0x95, 0x01,        //     Report Count (1)
    0x81, 0x06,        //     Input (Data,Var,Rel)        ; horizontal wheel
    0xC0,              //   End Collection (Physical)
    0xC0,              // End Collection (Application)
};

// ---------------------------------------------------------------------------
// Keyboard
//
// Two collections selected by report id:
//   ID 1 - boot keyboard: 8 modifier bits (LCtrl..RGui), a reserved byte, and
//          six key-code slots (6-key rollover, HID Keyboard/Keypad usages).
//   ID 2 - consumer control: one 16-bit consumer usage (media/volume keys), 0 = none.
// Input-only for now (no lock-LED output report); LED feedback arrives with the
// output-event channel in a later milestone. Report bytes after the id:
//   keyboard - modifiers(1) + reserved(1) + keys(6) = 8  (9 with the id)
//   consumer - usage(2)                                  (3 with the id)
// ---------------------------------------------------------------------------
static const UCHAR k_KeyboardReportDescriptor[] = {
    // ---- Boot keyboard (Report ID 1) ----
    0x05, 0x01,        // Usage Page (Generic Desktop)
    0x09, 0x06,        // Usage (Keyboard)
    0xA1, 0x01,        // Collection (Application)
    0x85, 0x01,        //   Report ID (1)
    0x05, 0x07,        //   Usage Page (Keyboard/Keypad)
    0x19, 0xE0,        //   Usage Minimum (Left Control)
    0x29, 0xE7,        //   Usage Maximum (Right GUI)
    0x15, 0x00,        //   Logical Minimum (0)
    0x25, 0x01,        //   Logical Maximum (1)
    0x75, 0x01,        //   Report Size (1)
    0x95, 0x08,        //   Report Count (8)
    0x81, 0x02,        //   Input (Data,Var,Abs)        ; 8 modifier bits
    0x95, 0x01,        //   Report Count (1)
    0x75, 0x08,        //   Report Size (8)
    0x81, 0x03,        //   Input (Const,Var,Abs)       ; reserved byte
    0x95, 0x06,        //   Report Count (6)
    0x75, 0x08,        //   Report Size (8)
    0x15, 0x00,        //   Logical Minimum (0)
    0x26, 0xFF, 0x00,  //   Logical Maximum (255)
    0x05, 0x07,        //   Usage Page (Keyboard/Keypad)
    0x19, 0x00,        //   Usage Minimum (0)
    0x29, 0xFF,        //   Usage Maximum (255)
    0x81, 0x00,        //   Input (Data,Array)          ; 6 key-code slots
    0xC0,              // End Collection

    // ---- Consumer control (Report ID 2) ----
    0x05, 0x0C,        // Usage Page (Consumer)
    0x09, 0x01,        // Usage (Consumer Control)
    0xA1, 0x01,        // Collection (Application)
    0x85, 0x02,        //   Report ID (2)
    0x19, 0x00,        //   Usage Minimum (0)
    0x2A, 0x3C, 0x02,  //   Usage Maximum (0x023C)
    0x15, 0x00,        //   Logical Minimum (0)
    0x26, 0x3C, 0x02,  //   Logical Maximum (0x023C)
    0x95, 0x01,        //   Report Count (1)
    0x75, 0x10,        //   Report Size (16)
    0x81, 0x00,        //   Input (Data,Array)          ; one consumer usage (0 = none)
    0xC0,              // End Collection
};

// ---------------------------------------------------------------------------
// Registry. Types absent here return NULL from VoidInputGetDeviceDesc and so
// fail CREATE with STATUS_NOT_SUPPORTED until their milestone lands.
// ---------------------------------------------------------------------------
static const VOIDINPUT_DEVICE_DESC k_Devices[] = {
    {
        VoidInputDeviceMouse,
        0x1BCF, 0x0005, 0x0100,        // generic optical-mouse identity (override via CREATE)
        k_MouseReportDescriptor,
        (USHORT)sizeof(k_MouseReportDescriptor),
        TRUE,                          // numbered reports (ids 1 and 2)
        TRUE,                          // singleton
        FALSE,                         // not a gamepad
        VOIDINPUT_EVT_NONE,            // input-only
    },
    {
        VoidInputDeviceKeyboard,
        0x1A2C, 0x0042, 0x0100,        // generic USB-keyboard identity (override via CREATE)
        k_KeyboardReportDescriptor,
        (USHORT)sizeof(k_KeyboardReportDescriptor),
        TRUE,                          // numbered reports (ids 1 and 2)
        TRUE,                          // singleton
        FALSE,                         // not a gamepad
        VOIDINPUT_EVT_NONE,            // input-only (lock-LED output is a later milestone)
    },
};

const VOIDINPUT_DEVICE_DESC* VoidInputGetDeviceDesc(VOIDINPUT_DEVICE_TYPE type)
{
    for (size_t i = 0; i < ARRAYSIZE(k_Devices); ++i) {
        if (k_Devices[i].Type == type) {
            return &k_Devices[i];
        }
    }
    return NULL;   // type not implemented yet
}
