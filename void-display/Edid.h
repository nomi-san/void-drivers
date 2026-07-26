/*
 * VoidDisplay - EDID generation.
 *
 * Produces a valid 128-byte EDID 1.4 base block branded "VVD" / "VoidDisplay".
 * The advertised mode list is supplied separately through the IddCx
 * ParseMonitorDescription / QueryTargetModes callbacks, so this block only has
 * to be a structurally valid, identity-carrying EDID. The serial argument keeps
 * each monitor's EDID distinct so the OS does not collapse multiple virtual
 * monitors into one.
 */

#pragma once

#include <windows.h>
#include "Public.h"   // VOIDDISPLAY_MAX_DISPLAYS

#define VOIDDISPLAY_EDID_SIZE 128
#define VOIDDISPLAY_EDID_MAX  256   // base block + one CTA extension

/*
 * Base of the per-slot serial written to EDID bytes 12-15 (serial = base + slot).
 * Distinct serials stop the OS collapsing several virtual monitors into one, and
 * they double as the only way to tell WHICH monitor a description belongs to -
 * see VoidEdidSlotFromDescription.
 */
#define VOID_EDID_SERIAL_BASE 0x56564400u

void VoidBuildEdid(UINT8 out[VOIDDISPLAY_EDID_SIZE], UINT32 serial);

/*
 * Recover the slot index from an EDID this driver generated.
 *
 * EvtIddCxParseMonitorDescription is handed a description and nothing else - no
 * monitor object, no context - so the serial is the only channel back to the slot
 * that created it. Returns false when the block is too short or its serial is not
 * one of ours, which is the normal case for an operator-supplied display.edid
 * with PatchSerial=0. Callers fall back to the built-in default mode.
 */
bool VoidEdidSlotFromDescription(const UINT8* edid, UINT32 size, UINT32* outSlot);

/*
 * Load an operator-supplied EDID from C:\ProgramData\voidrv\display.edid (a dumped
 * real-monitor EDID). Returns the byte count (128 or 256) on success, or 0 if the
 * file is absent/invalid (caller falls back to VoidBuildEdid). Validates size,
 * header and checksum(s). When patchSerial, stamps a per-slot serial into bytes
 * 12-15 and fixes the base-block checksum so multiple monitors are not identical.
 */
UINT32 VoidLoadCustomEdid(UINT8 out[VOIDDISPLAY_EDID_MAX], UINT32 serial, bool patchSerial);
