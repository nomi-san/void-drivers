/*
 * VoidDisplay - default mode table and DISPLAYCONFIG signal-info helpers.
 *
 * Needs only <windows.h> (DISPLAYCONFIG_* live in wingdi.h); the IddCx-specific
 * mode structures are filled in by the caller using these helpers.
 */

#pragma once

#include <windows.h>

typedef struct _VOID_MODE_DESC {
    UINT32 Width;
    UINT32 Height;
    UINT32 RefreshHz;
} VOID_MODE_DESC;

/* Compiled-in set of modes advertised on every monitor. */
extern const VOID_MODE_DESC g_VoidDefaultModes[];
extern const unsigned       g_VoidDefaultModeCount;

/* Default mode used when an ADD request leaves the mode zeroed. */
#define VOID_DEFAULT_WIDTH      1920
#define VOID_DEFAULT_HEIGHT     1080
#define VOID_DEFAULT_REFRESH    60

/*
 * Build a DISPLAYCONFIG_VIDEO_SIGNAL_INFO for a (w, h, vsync) mode.
 *
 * vSyncFreqDivider rule (from IddCx): pass 0 for a monitor mode
 * (IDDCX_MONITOR_MODE), pass a non-zero value (1) for a target mode
 * (IDDCX_TARGET_MODE). The two paths must not share the same value.
 */
DISPLAYCONFIG_VIDEO_SIGNAL_INFO VoidCreateSignalInfo(UINT32 width, UINT32 height,
                                                     UINT32 vsync, UINT32 vSyncFreqDivider);

/* Index of (w, h, vsync) within g_VoidDefaultModes, or 0 if not present. */
unsigned VoidFindModeIndex(UINT32 width, UINT32 height, UINT32 vsync);
