/* Copyright The Phorepoc Authors
 * SPDX-License-Identifier: EPL-2.0 OR GPL-2.0-or-later
 *
 * unistd.h -- the libc header, minus its getopt globals: P.I.P.S. defines optarg/optind/... as macros (thread-local
 * accessors), which break zsh, a program with its own local variables of those names and no getopt() calls. */
#include_next <unistd.h>
#undef optarg
#undef optind
#undef opterr
#undef optopt
