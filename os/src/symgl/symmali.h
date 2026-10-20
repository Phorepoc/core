/* Copyright The Phorepoc Authors
 * SPDX-License-Identifier: MIT -- the seam headers mesa-symbian (MIT) compiles against */

/* symmali.h -- OURS (G4): the C seam between the DRM shim (mesa-symbian: symbian/src/drm_symbian.c, C) and the
 * Symbian driver channel (src/symgl/symgpu_null.cpp here; a GPU board brings its own, e.g. adaptation-a64 src/symgl/symmali.cpp).
 * Errors are Symbian codes (negative). */
#ifndef SYMMALI_H
#define SYMMALI_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct symmali symmali_t;

typedef struct {
   uint32_t gp_version, pp_version;
   int num_pp;
   uint32_t arena_va;          /* GPU address of arena offset 0 */
   uint32_t arena_max;         /* bytes */
   void *arena_base;           /* CPU address of arena offset 0 (uncached, write-combining) */
} symmali_info_t;

symmali_t *symmali_open(void);                       /* NULL: no Mali driver in this kernel */
void symmali_close(symmali_t *m);
void symmali_info(symmali_t *m, symmali_info_t *out);
int symmali_commit(symmali_t *m, uint32_t offset, uint32_t size);
int symmali_import(symmali_t *m, int chunk_handle, uint32_t offset, uint32_t size, uint32_t *va);
int symmali_import_addr(symmali_t *m, const void *addr, uint32_t size, uint32_t *va);   /* G6b: chunk found by address */
int symmali_unimport(symmali_t *m, uint32_t va);
int symmali_cache_sync(symmali_t *m, uint32_t va, uint32_t size, int op);

/* one job per pipe in flight: submit (frame = the lima register image, 6 words for GP, 68 for PP), then wait.
 * wait: poll = 0 blocks until the job is done and returns its result; poll = 1 returns 1 while it still runs */
#define SYMMALI_PIPE_GP 0
#define SYMMALI_PIPE_PP 1
int symmali_submit(symmali_t *m, int pipe, const uint32_t *frame, unsigned words);
int symmali_wait(symmali_t *m, int pipe, int poll);

void symmali_log(const char *msg);

#ifdef __cplusplus
}
#endif

#endif
