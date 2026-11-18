/* Copyright The Phorepoc Authors
 * SPDX-License-Identifier: Apache-2.0
 *
 * console.h -- OURS: NimBLE's console hook (ble_gatts_lcl.c prints the GATT database with it). No console in the
 * Bluetooth stack's process: the output is dropped.
 */
#ifndef PHE_NIMBLE_CONSOLE_H
#define PHE_NIMBLE_CONSOLE_H
#define console_printf(...) ((void)0)
#endif
