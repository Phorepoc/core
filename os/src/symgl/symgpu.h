/* Copyright The Phorepoc Authors
 * SPDX-License-Identifier: MIT -- a seam header GPU drivers of any licence (Mesa's: MIT) compile against */

/* symgpu.h -- OURS (G4): the C seam between a GPU's user-side driver (a Mesa port's DRM shim, C) and the Symbian
 * driver channel to its kernel driver (src/symgl/symgpu_null.cpp here; a SoC with a GPU brings its own through its
 * registry's EXTEND table). The job model is that of tile-based GPUs with separate geometry and pixel pipes.
 * Errors are Symbian codes (negative). */
#ifndef SYMGPU_H
#define SYMGPU_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct symgpu symgpu_t;

typedef struct {
   uint32_t gp_version, pp_version;
   int num_pp;
   uint32_t arena_va;          /* GPU address of arena offset 0 */
   uint32_t arena_max;         /* bytes */
   void *arena_base;           /* CPU address of arena offset 0 (uncached, write-combining) */
} symgpu_info_t;

symgpu_t *symgpu_open(void);                       /* NULL: no Mali driver in this kernel */
void symgpu_close(symgpu_t *m);
void symgpu_info(symgpu_t *m, symgpu_info_t *out);
int symgpu_commit(symgpu_t *m, uint32_t offset, uint32_t size);
int symgpu_import(symgpu_t *m, int chunk_handle, uint32_t offset, uint32_t size, uint32_t *va);
int symgpu_import_addr(symgpu_t *m, const void *addr, uint32_t size, uint32_t *va);   /* G6b: chunk found by address */
int symgpu_unimport(symgpu_t *m, uint32_t va);
int symgpu_cache_sync(symgpu_t *m, uint32_t va, uint32_t size, int op);

/* one job per pipe in flight: submit (frame = the lima register image, 6 words for GP, 68 for PP), then wait.
 * wait: poll = 0 blocks until the job is done and returns its result; poll = 1 returns 1 while it still runs */
#define SYMGPU_PIPE_GP 0
#define SYMGPU_PIPE_PP 1
int symgpu_submit(symgpu_t *m, int pipe, const uint32_t *frame, unsigned words);
int symgpu_wait(symgpu_t *m, int pipe, int poll);

void symgpu_log(const char *msg);

#ifdef __cplusplus
}
#endif

#endif
