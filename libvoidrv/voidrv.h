/*
 * libvoidrv - user-mode SDK for the Void drivers.
 *
 * Public, self-contained C ABI. A host app links libvoidrv and talks to the
 * drivers through these functions; it never builds raw IOCTL buffers. C++ is
 * fine too - everything is declared extern "C".
 *
 * This header intentionally does not expose the driver wire format (that lives
 * in the driver's Public.h); the SDK ABI is decoupled from it.
 */

#ifndef VOIDRV_H
#define VOIDRV_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

#define VOIDRV_MAX_DISPLAYS 8
#define VOIDRV_MAX_MODES    128

typedef struct VoidrvDisplayMode {
    uint32_t Width;      /* pixels; 0 in an Add request selects the driver default */
    uint32_t Height;     /* pixels */
    uint32_t RefreshHz;  /* nominal refresh, e.g. 60, 120, 144, 240 */
} VoidrvDisplayMode;

typedef struct VoidrvModeList {
    uint32_t          Count;
    VoidrvDisplayMode Modes[VOIDRV_MAX_MODES];
} VoidrvModeList;

typedef struct VoidrvDisplayEntry {
    uint32_t          InUse;
    VoidrvDisplayMode Mode;
} VoidrvDisplayEntry;

typedef struct VoidrvDisplayState {
    uint32_t           Count;
    VoidrvDisplayEntry Entries[VOIDRV_MAX_DISPLAYS];
} VoidrvDisplayState;

typedef enum VoidrvStatus {
    VOIDRV_STATUS_OK = 0,        /* present and openable */
    VOIDRV_STATUS_NOT_INSTALLED, /* device interface not found */
    VOIDRV_STATUS_INACCESSIBLE   /* found but could not be opened */
} VoidrvStatus;

/* Opaque control-channel handle. */
typedef struct VoidrvDisplay* VoidrvDisplayHandle;

/* Is the VoidDisplay control interface present? */
VoidrvStatus        VoidrvDisplayQueryStatus(void);

/* Open / close the control channel. Open returns NULL on failure (GetLastError). */
VoidrvDisplayHandle VoidrvDisplayOpen(void);
void                VoidrvDisplayClose(VoidrvDisplayHandle handle);

/* Driver interface version, or 0 on failure. */
uint32_t            VoidrvDisplayVersion(VoidrvDisplayHandle handle);

/* Add a display. Pass NULL or a zeroed mode for the driver default. */
/* Returns the new slot index (>= 0), or -1 on failure. */
int                 VoidrvDisplayAdd(VoidrvDisplayHandle handle, const VoidrvDisplayMode* mode);

/* Remove a display by slot index. */
bool                VoidrvDisplayRemove(VoidrvDisplayHandle handle, uint32_t index);

/* Change a display's mode. */
bool                VoidrvDisplaySetMode(VoidrvDisplayHandle handle, uint32_t index,
                                         const VoidrvDisplayMode* mode);

/* Read the current display table. */
bool                VoidrvDisplayList(VoidrvDisplayHandle handle, VoidrvDisplayState* state);

/* Add a custom mode to the advertised list (visible in Windows display settings).
   Built-in default modes are always advertised. Live displays re-plug to pick it up. */
bool                VoidrvDisplayAddMode(VoidrvDisplayHandle handle, const VoidrvDisplayMode* mode);

/* Remove a previously added custom mode (defaults cannot be removed). */
bool                VoidrvDisplayRemoveMode(VoidrvDisplayHandle handle, const VoidrvDisplayMode* mode);

/* Read the full advertised mode list (defaults + custom). */
bool                VoidrvDisplayListModes(VoidrvDisplayHandle handle, VoidrvModeList* list);

/* Whether the SDK can persist changes so they survive an adapter restart / reboot.
   Persistence writes HKLM and needs elevation. When this is false, Add/Remove/
   SetMode/AddMode/RemoveMode still take effect for the current session but are NOT
   saved (the driver reads the saved set at init; an unelevated caller cannot write
   it). A host app can call this to decide whether to elevate. */
bool                VoidrvDisplayPersistenceWritable(void);

/* ===================== VoidInput ===================================== */

/* Virtual input device types (mirrors the driver's type enum). */
typedef enum VoidrvInputType {
    VOIDRV_INPUT_MOUSE    = 1,  /* relative + absolute pointer */
    VOIDRV_INPUT_KEYBOARD = 2,
    VOIDRV_INPUT_XBOXONE  = 3,
    VOIDRV_INPUT_DS4      = 4,
    VOIDRV_INPUT_DS5      = 5,
    VOIDRV_INPUT_TOUCH    = 6,
} VoidrvInputType;

/* Mouse button bitmask. */
#define VOIDRV_MB_LEFT    0x01
#define VOIDRV_MB_RIGHT   0x02
#define VOIDRV_MB_MIDDLE  0x04
#define VOIDRV_MB_X1      0x08
#define VOIDRV_MB_X2      0x10

/* Opaque per-device handle. Each handle owns exactly one virtual device; the
   device exists until the handle is closed. */
typedef struct VoidrvInput* VoidrvInputHandle;

/* Is the VoidInput control interface present? */
VoidrvStatus      VoidrvInputQueryStatus(void);

/* Driver interface version, or 0 on failure. Opens and closes its own handle. */
uint32_t          VoidrvInputVersion(void);

/* Create a virtual input device of the given type. Returns NULL on failure
   (GetLastError). Close the handle to remove the device. */
VoidrvInputHandle VoidrvInputCreate(VoidrvInputType type);

/* Remove the device and close its handle. */
void              VoidrvInputClose(VoidrvInputHandle handle);

/* Mouse - relative move. dx/dy are signed deltas; buttons is a VOIDRV_MB_*
   bitmask; wheel/hwheel are signed detents. Requires a mouse handle. */
bool              VoidrvInputMouseMoveRelative(VoidrvInputHandle handle,
                                               int16_t dx, int16_t dy,
                                               uint8_t buttons,
                                               int8_t wheel, int8_t hwheel);

/* Mouse - absolute position, normalized 0..32767 across the desktop. */
bool              VoidrvInputMouseMoveAbsolute(VoidrvInputHandle handle,
                                               uint16_t x, uint16_t y,
                                               uint8_t buttons,
                                               int8_t wheel, int8_t hwheel);

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* VOIDRV_H */
