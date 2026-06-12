/*
 * VoidDisplay - shared control interface.
 *
 * This header is the contract between the driver and user-mode callers
 * (libvoidrv / voidctl). It is plain C and may be included from either side.
 * The includer is expected to have pulled in <windows.h> first.
 */

#pragma once

#include <winioctl.h>
#include <guiddef.h>

/* Control device interface exposed by VoidDisplay. */
/* {40255101-a910-441c-84d6-9f027197fa70} */
DEFINE_GUID(GUID_DEVINTERFACE_VOIDDISPLAY,
    0x40255101, 0xa910, 0x441c, 0x84, 0xd6, 0x9f, 0x02, 0x71, 0x97, 0xfa, 0x70);

#define VOIDDISPLAY_VERSION       1
#define VOIDDISPLAY_MAX_DISPLAYS  8

#define VOIDDISPLAY_IOCTL(i, access) \
    CTL_CODE(FILE_DEVICE_UNKNOWN, 0x800 + (i), METHOD_BUFFERED, (access))

/* Query interface/driver version. Out: ULONG (VOIDDISPLAY_VERSION). */
#define IOCTL_VOIDDISPLAY_VERSION   VOIDDISPLAY_IOCTL(1, FILE_READ_ACCESS | FILE_WRITE_ACCESS)

/* Add a display. In: VOIDDISPLAY_MODE. Out: ULONG slot index (0..MAX-1). */
#define IOCTL_VOIDDISPLAY_ADD       VOIDDISPLAY_IOCTL(2, FILE_READ_ACCESS | FILE_WRITE_ACCESS)

/* Remove a display. In: ULONG slot index. */
#define IOCTL_VOIDDISPLAY_REMOVE    VOIDDISPLAY_IOCTL(3, FILE_WRITE_ACCESS)

/* Change a display's mode. In: VOIDDISPLAY_SET_MODE. */
#define IOCTL_VOIDDISPLAY_SET_MODE  VOIDDISPLAY_IOCTL(4, FILE_WRITE_ACCESS)

/* List current displays. Out: VOIDDISPLAY_STATE. */
#define IOCTL_VOIDDISPLAY_LIST      VOIDDISPLAY_IOCTL(5, FILE_READ_ACCESS | FILE_WRITE_ACCESS)

#include <pshpack1.h>

typedef struct _VOIDDISPLAY_MODE {
    UINT32 Width;      /* pixels; 0 in an ADD request means "use the default mode" */
    UINT32 Height;     /* pixels */
    UINT32 RefreshHz;  /* nominal refresh, e.g. 60, 120, 144, 240 */
} VOIDDISPLAY_MODE;

typedef struct _VOIDDISPLAY_SET_MODE {
    UINT32 Index;
    VOIDDISPLAY_MODE Mode;
} VOIDDISPLAY_SET_MODE;

typedef struct _VOIDDISPLAY_ENTRY {
    UINT32 InUse;      /* 0 or 1 */
    VOIDDISPLAY_MODE Mode;
} VOIDDISPLAY_ENTRY;

typedef struct _VOIDDISPLAY_STATE {
    UINT32 Count;      /* number of displays in use */
    VOIDDISPLAY_ENTRY Entries[VOIDDISPLAY_MAX_DISPLAYS];
} VOIDDISPLAY_STATE;

#include <poppack.h>
