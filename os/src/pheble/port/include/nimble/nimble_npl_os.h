/* Copyright The Phorepoc Authors
 * SPDX-License-Identifier: Apache-2.0
 *
 * nimble_npl_os.h -- OURS: the Symbian NPL (porting/npl/symbian). Implemented in port/src/npl_symbian.cpp.
 */
#ifndef _NIMBLE_NPL_OS_H_
#define _NIMBLE_NPL_OS_H_

#include <assert.h>
#include <stdint.h>
#include <string.h>
#include <limits.h>
#include "os_types.h"

#ifdef __cplusplus
extern "C" {
#endif

#define BLE_NPL_OS_ALIGNMENT 4
/* C++ without __STDC_LIMIT_MACROS: the limits NimBLE's headers use */
#ifndef UINT32_MAX
#define UINT32_MAX 0xffffffffU
#endif
#ifndef INT32_MAX
#define INT32_MAX 0x7fffffff
#endif
#ifndef INT32_MIN
#define INT32_MIN (-INT32_MAX - 1)
#endif
#ifndef UINT16_MAX
#define UINT16_MAX 0xffff
#endif
#ifndef INT16_MAX
#define INT16_MAX 0x7fff
#endif
#ifndef UINT8_MAX
#define UINT8_MAX 0xff
#endif
#define BLE_NPL_TIME_FOREVER UINT32_MAX

struct ble_npl_eventq *ble_npl_eventq_dflt_get(void);
void ble_npl_eventq_run(struct ble_npl_eventq *evq);

/* Phorepoc: create the port's process-wide lock and timer thread; call once before nimble_port_init(). */
int phe_npl_init(void);

#ifdef __cplusplus
}
#endif

#endif /* _NIMBLE_NPL_OS_H_ */
