# toolchain/efile_exe — EXE-emission seam TUs for the from-source EFILE.EXE

Support TUs for `emitter/build_efile_exe.py` (Track B / M7). The EXE itself is
the 48 unmodified 2010 fileserver TUs (`tools/kernel/efile_compile_check.sh`)
plus the REAL `uc_exe.cia` (exexp entry) and `common/gcchelp.cpp` (operator
new/delete); these two files are the only non-baseline code in the image:

- `dev_exe_startup.cpp` — RunThread for modern GCC (uc_exe.cpp only has
  __GCC32__/__ARMCC__ bodies): heap setup, `User::InitProcess`, the
  `.init_array` static-ctor walk (bracketed by `emitter/efile.ld`), `E32Main`.
- `crt_shim.cpp` — `__cxa_pure_virtual` / `__aeabi_atexit` / `__dso_handle` /
  sized operator delete; everything else C++-runtime comes from libgcc.

