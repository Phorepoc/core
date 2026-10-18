# core

The shared part of Phorepoc OS: the build tools and the component registry that turn the Symbian source (plus our
changes to it and our own components) into images, for x86-64 (x32) and ARM. Everything runs on Linux, inside the
[Phorepoc toolchain image](https://github.com/Phorepoc/toolchain) (`ghcr.io/phorepoc/base`).

This is **phase A: the build tools**. The OS overlay (our changed Symbian files) and our own components arrive in
phase B, in `os/`; the device repos (adaptations) pin a tagged release of this one.

## Layout

```
ph                       the task runner (./ph help)
sources.lock, fetch.sh   the Symbian source: pinned commits of the Phorepoc forks, fetched into symbian/
tools/
  emitter/               the component registry and the builders
    paths.py               where everything lives (see "Paths")
    components*.py         the registry: one entry per component, built from its real .mmp
    mmp.py                 .mmp parser (preprocessor, platform macros)
    build_mmp.py           ARM component builder          build_mmp_x32.py   x32 (amd64) component builder
    elf.py, e32image.py    ELF reader and E32Image writer (what elf2e32 does), bytepair.py / e32deflate.py compression
    devlink.py, euser_def.py, loadercheck.py    import/export tables from the frozen .def files, loader checks
    build_euser_*.py, build_efsrv_*.py, build_hal_*.py, build_efile_*.py    the providers every image imports from
    sis.py                 SIS writer
    *.ld                   linker scripts
  toolchain/             compiler front end: symcc (the codegen rules), cc_batch.py (parallel compile), cc_cached, genexec.pl
  epoc32_export.py       what `abld export` does: the epoc32/include tree from the bld.inf files
  casefix.py             case-insensitive include spellings on a case-sensitive file system
  rsc.py, ecom_rsc.py    resource compiler driver (rcomp)
  rombuild.py            XIP ROM image builder (prelinker); x32_rom_args.py, gui_rom_args.py ROM contents
  rom_heap.tsv           heap and stack sizes of the ROM executables
  mif_gen.py, font_gen.py, zdata_gen.py, features_dat.py, ...    icon, font, Z: data and feature generators
tests/
  compare_images.py      byte-compare images built here with another build of the same components
  compare_*.tsv          the pairs
```

## Paths

| variable | meaning | default |
|---|---|---|
| `PHOREPOC_SYMBIAN` | Symbian source: `symbian-2010/<package>`, `symbian-rcl3/<package>` | `symbian/` (filled by `fetch.sh`) |
| `PHOREPOC_BUILD` | every generated file: `epoc32/`, `casefix/`, `x32img/`, `arm/`, `rsc/` ... | `build/` |
| `PHOREPOC_OS_ROOT` | our overlay and components: `toolchain/{shims,overlay,kinc,generated}`, `src/`, the board and port files | `os/` |

`PHOREPOC_OS_ROOT` is temporary: until phase B moves the overlay and our components into `os/`, it has to point at a
checkout that still holds them. The registry itself is complete; entries whose files are not there cannot be built yet.

## Building

```sh
./fetch.sh                  # Symbian source -> symbian/
./ph export                 # epoc32/include tree
./ph casefix                # case-insensitive include links
./ph build palette estor    # x32 images -> build/x32img/
./ph arm palette estor      # ARM images
./ph help
```

Every command runs in the toolchain container (or directly when already inside one). Build output is deterministic:
the same inputs give the same bytes, which `tests/compare_images.py` checks against another build.

## Licence

Eclipse Public License 1.0 (see `LICENSE`).
