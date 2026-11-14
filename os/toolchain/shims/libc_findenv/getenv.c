/* Copyright The Phorepoc Authors
 * SPDX-License-Identifier: EPL-2.0 OR GPL-2.0-or-later */

/* OURS: libc's getenv.c with __findenv EMITTED. The release declares it `inline char *__findenv(...)` in getenv.c and
 * calls it (as an extern) from setenv.c/unsetenv; RVCT emitted the inline definition, GCC (C++ driver, -O2) inlines it
 * into getenv() and emits no out-of-line copy, so libc.dll's exported __findenv slot stayed an unimplemented stub and
 * setenv()/unsetenv() died EUSRNIMP (QuasarMX, 2026-10-14: qputenv -> setenv -> __findenv). */
#undef inline
#define inline
#define __inline   /* a prefix/e32 header maps `inline` to __inline */
#include <symbian-2010/oss.FCL.sf.os.ossrv/genericopenlibs/openenvcore/libc/src/stdlib/getenv.c>
