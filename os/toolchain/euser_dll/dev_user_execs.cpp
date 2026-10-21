// Copyright The Phorepoc Authors
// SPDX-License-Identifier: Apache-2.0

/*
 * dev_user_execs.cpp — the DEVICE euser exec layer (Track B / M7).
 *
 * Emits the generated user-side exec stub BODIES for the REAL from-source kernel
 * (QEMU/Syborg), NOT the EKA2L1 HLE bench. u32exec.h unconditionally pulls
 * <exec_enum.h> + <exec_user.h>; exec_user.h's 281 stub bodies are guarded by
 * __GEN_USER_EXEC_CODE__, so this one TU (compiled with that macro) defines the
 * whole user-side executive:  Exec::* (SLOW_EXEC/FAST_EXEC = device `svc <n>`),
 * E32Loader::* (all 27 — the file-server loader boundary: CodeSegCreate/
 * ProcessCreate/CodeSegOpen/... -> ExecHandler::* in the from-source scodeseg.cpp),
 * and UserSvr::* (RomHeaderAddress/ExecuteInSupervisorMode/FsRegisterThread/...).
 *
 * Why this is a SEPARATE TU from the EKA2L1-bench euser (emitter/build_euser_dll.py):
 * that build deliberately does NOT pull exec_user.h — its exec seams
 * (toolchain/euser_dll/{sched_env,sync_env,...}) bind the bench's HLE svc numbers
 * by hand, because the EKA2L1 v10 executive uses different svc numbers than the
 * device dispatch table. For a euser that runs on OUR kernel the device numbers
 * (exec_user.h) are exactly right, so the device euser uses THIS TU in place of
 * those HLE seams. See docs/track-b/efile-link.md ("device vs bench euser").
 *
 * Build (proven 2026-09-18, arm-none-eabi-g++ 14.2 via symcc, ARM):
 *   SYMCC_ARM=1 bash toolchain/symcc toolchain/euser_dll/dev_user_execs.cpp -O2 \
 *     -D__GEN_USER_EXEC_CODE__ \
 *     -I baseline/kernelhwsrv/kernel/eka/include -I toolchain/kinc -I toolchain/generated \
 *     -o dev_user_execs.o
 *   => 281 exec stub defs (E32Loader x27, UserSvr x5, Exec::* the rest).
 */
#include <e32std.h>
#include <e32base.h>
#include <e32svr.h>
#include <e32ldr.h>
#include <e32ldr_private.h>
#include <u32exec.h>   // -> exec_enum.h + exec_user.h; bodies emit under the macro
