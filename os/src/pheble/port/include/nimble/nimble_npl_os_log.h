/* Copyright The Phorepoc Authors
 * SPDX-License-Identifier: Apache-2.0
 *
 * nimble_npl_os_log.h -- OURS: NimBLE's log functions on Symbian. Every level goes to phe_nimble_vlog (port/src/npl_symbian.cpp),
 * which formats with vsnprintf and writes RDebug (the debug port / serial log). Levels below
 * MYNEWT_VAL(BLE_HS_LOG_LVL) are compiled out by the callers' own checks.
 */
#ifndef _NIMBLE_NPL_OS_LOG_H_
#define _NIMBLE_NPL_OS_LOG_H_

#include <stdarg.h>

#ifdef __cplusplus
extern "C" {
#endif
void phe_nimble_vlog(int lvl, const char *fmt, va_list args);
#ifdef __cplusplus
}
#endif

#define PHE_NIMBLE_LVL_DEBUG 0
#define PHE_NIMBLE_LVL_INFO 1
#define PHE_NIMBLE_LVL_WARN 2
#define PHE_NIMBLE_LVL_ERROR 3
#define PHE_NIMBLE_LVL_CRITICAL 4

#define BLE_NPL_LOG_IMPL(lvl)                                                 \
    static inline void _BLE_NPL_LOG_CAT(                                      \
        BLE_NPL_LOG_MODULE, _BLE_NPL_LOG_CAT(_, lvl))(const char *fmt, ...)   \
    {                                                                         \
        va_list args;                                                         \
        if (PHE_NIMBLE_LVL_ ## lvl < MYNEWT_VAL(BLE_HS_LOG_LVL)) return;      \
        va_start(args, fmt);                                                  \
        phe_nimble_vlog(PHE_NIMBLE_LVL_ ## lvl, fmt, args);                   \
        va_end(args);                                                         \
    }

#endif /* _NIMBLE_NPL_OS_LOG_H_ */
