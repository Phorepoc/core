#!/bin/bash
# Copyright The Phorepoc Authors
# SPDX-License-Identifier: Apache-2.0
# gen_def.sh -- regenerates pheeh.def from the x32 toolchain archives (run in the build image, stdout = the .def).
# Ordinals follow the sorted names, so a regenerated def renumbers: rebuild every eh component after it. Vtables and
# data objects are marked DATA; typeinfo (_ZTI/_ZTS) is left out (the importing module builds its own leaves).
set -e
G=$(gcc -mx32 -print-file-name=libgcc_eh.a); S=$(gcc -mx32 -print-file-name=libsupc++.a)
T=$(mktemp -d); cd $T
ar x $S $(for m in eh_alloc eh_call eh_catch eh_exception eh_personality eh_ptr eh_term_handler eh_terminate eh_throw eh_type eh_unex_handler class_type_info si_class_type_info vmi_class_type_info tinfo tinfo2 fundamental_type_info pbase_type_info pmem_type_info pointer_type_info function_type_info enum_type_info array_type_info dyncast bad_cast bad_typeid bad_alloc bad_array_length bad_array_new; do echo $m.o; done)
ar x $G unwind-dw2.o unwind-dw2-fde-dip.o
{ nm -g --defined-only *.o 2>/dev/null | awk 'NF==3 && $2 ~ /^[TDRVBW]$/ {print $2, $3}' ;
  echo "T __cxa_get_globals"; echo "T __cxa_get_globals_fast"; echo "T _ZN9__gnu_cxx27__verbose_terminate_handlerEv"; } \
  | grep -v ' _ZT[IS]' | grep -v ' DW.ref\| __dso_handle' | sort -u -k2 > syms.txt
echo EXPORTS
i=0
while read -r t name; do
  i=$((i+1))
  case "$name" in _ZTV*) f=" DATA";; *) case "$t" in T|W) f="";; *) f=" DATA";; esac;; esac
  printf '\t%s @ %d NONAME%s\n' "$name" $i "$f"
done < syms.txt
