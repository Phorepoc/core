/* Copyright The Phorepoc Authors
 * SPDX-License-Identifier: MIT -- a seam header GPU drivers of any licence (Mesa's: MIT) compile against */

/* symvmw.h -- OURS (S3): the C seam between Mesa's svga driver (its DRM shim, drm_symbian_vmw.c, in the Mesa port) and the Symbian driver
 * channel to the VMware SVGA II / SVGA3D kernel driver (any hypervisor that emulates that device with 3D). A board that has that device
 * brings the implementation (its src/symgl/symvmw.cpp over its driver channel); a board without it needs none, because only a Mesa build with
 * the svga driver calls these. Errors are Symbian codes (negative).
 *
 * The model is the kernel half of vmwgfx in small: guest memory is a POOL (one shared chunk of this process, committed page by page); a range of
 * it becomes a MOB (the device's guest-backed object memory) with symvmw_mob_define; command buffers are staged in the ARENA (a small physically
 * contiguous block) and run synchronously by symvmw_submit. Object ids (MOB, surface, DX context) are device-wide: this process owns a window of
 * each (symvmw_info_t). */
#ifndef SYMVMW_H
#define SYMVMW_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define SYMVMW_DEVCAPS 262   /* SVGA3D_DEVCAP_MAX */

typedef struct symvmw symvmw_t;

typedef struct {
   uint32_t caps, cap2;               /* SVGA_REG_CAPABILITIES / CAP2 */
   uint32_t devcap_3d, devcap_dx, devcap_sm41, devcap_sm5, devcap_gl43, max_texture;
   uint32_t mob_max_kb;               /* the device's largest MOB */
   uint32_t mob_memory;               /* bytes of guest memory this process may pin for the device: the pool, within the free RAM */
   void *pool_base;                   /* CPU address of pool offset 0 */
   uint32_t pool_bytes;               /* address space of the pool (pages are committed on demand) */
   void *arena_base;                  /* CPU address of arena offset 0 */
   uint32_t arena_bytes;
   uint32_t mob_base, mob_count;      /* this process's windows of the device's ids */
   uint32_t surf_base, surf_count;
   uint32_t ctx_base, ctx_count;
} symvmw_info_t;

/* NULL: no VMware SVGA II device in this kernel, 3D is off in the hypervisor or it has no guest-backed objects / DX contexts */
symvmw_t *symvmw_open(void);
void symvmw_close(symvmw_t *v);
void symvmw_info(symvmw_t *v, symvmw_info_t *out);
/* SVGA3D_DEVCAP_MAX device caps */
int symvmw_devcaps(symvmw_t *v, uint32_t *caps, unsigned count);
int symvmw_pool_commit(symvmw_t *v, uint32_t offset, uint32_t size);   /* page aligned */
int symvmw_mob_define(symvmw_t *v, uint32_t mobid, uint32_t offset, uint32_t size);
int symvmw_mob_destroy(symvmw_t *v, uint32_t mobid);
/* one command buffer: arena [offset, offset + length) on queue context `context` (0 = 3D); flags bit 1 = DX context `dx_context`.
   surf_high / ctx_high: how many surface / DX context ids of this process's windows are in use (for the cleanup at exit).
   status = SVGACBStatus (1 = completed) */
int symvmw_submit(symvmw_t *v, uint32_t offset, uint32_t length, uint32_t context, uint32_t flags, uint32_t dx_context,
                  uint32_t surf_high, uint32_t ctx_high, uint32_t *status, uint32_t *error_offset);

#ifdef __cplusplus
}
#endif

#endif
