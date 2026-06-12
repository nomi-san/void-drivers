/*
 * VoidDisplay - lightweight ASCII logging.
 *
 * Routes through OutputDebugStringA so the messages show up in DebugView.
 * No WPP/ETW for v1. All format strings must be ASCII only (no em/en dashes
 * or arrows) - see the project rules.
 */

#pragma once

#include <windows.h>
#include <stdio.h>

#if defined(DBG) && DBG
#define VOID_LOG_ENABLED 1
#elif defined(_DEBUG)
#define VOID_LOG_ENABLED 1
#else
#define VOID_LOG_ENABLED 0
#endif

#if VOID_LOG_ENABLED
#define VOID_LOG(fmt, ...)                                                      \
    do {                                                                        \
        char _voidLogBuf[512];                                                  \
        _snprintf_s(_voidLogBuf, sizeof(_voidLogBuf), _TRUNCATE,                \
                    "[VoidDisplay] " fmt "\n", ##__VA_ARGS__);                  \
        OutputDebugStringA(_voidLogBuf);                                        \
    } while (0)
#else
#define VOID_LOG(fmt, ...) ((void)0)
#endif
