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

#define VOIDDISPLAY_EDID_SIZE 128

void VoidBuildEdid(UINT8 out[VOIDDISPLAY_EDID_SIZE], UINT32 serial);
