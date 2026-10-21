# core

The Phorepoc OS and the data that describes it: our changes to Symbian and our own components (`os/`), the component
registry that says how each Symbian component is built from its real `.mmp`, and the pinned Symbian source. The tools
that turn these into images for x86-64 (x32) and ARM live in the [pdk](https://github.com/Phorepoc/pdk); build with its
`ph`.

What every board shares is here. Hardware lives in the adaptation repos, which are built together with core:

| adaptation | what |
|---|---|
| [adaptation-x86-64](https://github.com/Phorepoc/adaptation-x86-64) | the x86-64 kernel port (x32 ABI) |
| [adaptation-pc](https://github.com/Phorepoc/adaptation-pc) | PCs: QEMU, VirtualBox, real hardware |
| [adaptation-a64](https://github.com/Phorepoc/adaptation-a64) | the Allwinner A64 SoC drivers |
| [adaptation-pinephone](https://github.com/Phorepoc/adaptation-pinephone) | the PINE64 PinePhone |

Separate repositories, built in the same way when wanted: [mesa-symbian](https://github.com/Phorepoc/mesa-symbian)
(OpenGL ES through Mesa, MIT) and [codecpack](https://github.com/Phorepoc/codecpack) (FFmpeg and OpenH264, LGPL/BSD;
installed separately, never part of an image), and [tls](https://github.com/Phorepoc/tls)
(SSL.DLL and libssl.dll over mbedTLS, Apache-2.0; without it the 2010 TLS builds).

## Layout

```
os/
  toolchain/shims/       our replacement copies of Symbian source files, one directory per change
  toolchain/{overlay,kinc,generated,dll_rt,euser_dll}   headers and runtime pieces every image builds with
  src/                   our own components (pheshell, phetty, phgl, phvg ...) and their tests
  shim/                  stand-in headers for interfaces the 2010 release did not ship
  tools/                 OS data the ROM recipe reads (central repository files), third-party build recipes
registry/
  components.py          the registry: one entry per component, built from its real .mmp
  components_*.py        more entries: apps, development menu, phetty, Qt 4.8, Symbian^4
  rom_heap.tsv           heap and stack sizes of the ROM executables
  mif_dirs.txt           the directories whose icons go into .mif files
sources.lock, fetch.sh   the Symbian source: pinned commits of the Phorepoc forks, fetched into symbian/
```

## Paths

The pdk finds this repository through `PHOREPOC_CORE` (default: the `core` folder next to the pdk checkout).

| variable | meaning | default |
|---|---|---|
| `PHOREPOC_CORE` | this repository | `../core` from the pdk |
| `PHOREPOC_ADAPTATIONS` | the adaptation checkouts to build with core (a path list): their `os/` is laid over this one's, their `registry/` adds their components | none |
| `PHOREPOC_SYMBIAN` | Symbian source: `symbian-2010/<package>`, `symbian-rcl3/<package>` | `symbian/` (filled by `fetch.sh`) |
| `PHOREPOC_BUILD` | every generated file: `epoc32/`, `casefix/`, `x32img/`, `arm/`, `rsc/` ... | `build/` |
| `PHOREPOC_OS_ROOT` | this repository's OS tree | `os/` |

## Building

With the pdk's `ph` (its [command reference](https://github.com/Phorepoc/pdk/tree/main/docs/ph)), for the PC image:

```sh
ph source fetch                        # Symbian source -> core/symbian/
ph source export -a                    # the include tree (and the case links)
ph source mbg -a                       # icon id headers
ph component resources -A -b pc        # resources (.rsc + .rsg headers)
ph component build -A -b pc            # every component -> core/build/x32img/
ph image rom -b pc                     # the ROM
```

`-b pc` builds core with the PC's adaptation repositories (`ph board list`). Every command runs in the toolchain
container (or directly when already inside one). Build output is deterministic: the same inputs give the same bytes,
which `ph image compare` checks against another build.

## Licence

Apache-2.0 for our own code (see `LICENSES/Apache-2.0.txt`); the Nokia-derived files are our updated versions under EPL-2.0 with GPL-2.0-or-later as a Secondary License (`LICENSES/EPL-2.0.txt`; relicensed from EPL-1.0 under its section 7); unmodified Symbian sources are fetched from upstream and remain EPL-1.0, copyright The Phorepoc Authors (`AUTHORS`, `NOTICE`), except third-party files that say otherwise in their headers.

Contributions are welcome under the same terms: add yourself to `AUTHORS`.
