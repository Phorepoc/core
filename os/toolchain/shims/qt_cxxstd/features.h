/* Copyright The Phorepoc Authors
 * SPDX-License-Identifier: Apache-2.0 */
/* features.h -- OURS (S4.1): an empty stand-in for glibc's, which libstdc++'s Linux os_defines.h includes.
 * Qt on Symbian takes only libstdc++'s freestanding headers (<exception>, <typeinfo>, <new>) from the container
 * gcc, searched after everything of ours (emitter/components_s4.py); none of them needs glibc's feature macros. */
#ifndef __GLIBC_PREREQ
#define __GLIBC_PREREQ(maj, min) 0	/* os_defines.h asks: no glibc here */
#endif
