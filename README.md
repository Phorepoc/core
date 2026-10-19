# core

The Phorepoc OS and the data that describes it: the component registry that says how each Symbian component is
built from its real `.mmp`, and the pinned Symbian source. The tools that turn these into images for x86-64 (x32) and
ARM live in the [pdk](https://github.com/Phorepoc/pdk); build with its `ph`.

The OS overlay (our changed Symbian files) and our own components arrive in `os/`; the device repos (adaptations)
pin a tagged release of this one.

## Layout

```
registry/
  components.py          the registry: one entry per component, built from its real .mmp
  components_*.py        more entries: apps, codec pack, development menu, phetty, Qt 4.8, Symbian^4
  rom_heap.tsv           heap and stack sizes of the ROM executables
  mif_dirs.txt           the directories whose icons go into .mif files
sources.lock, fetch.sh   the Symbian source: pinned commits of the Phorepoc forks, fetched into symbian/
```

## Paths

The pdk finds this repository through `PHOREPOC_CORE` (default: the `core` folder next to the pdk checkout).

| variable | meaning | default |
|---|---|---|
| `PHOREPOC_CORE` | this repository | `../core` from the pdk |
| `PHOREPOC_SYMBIAN` | Symbian source: `symbian-2010/<package>`, `symbian-rcl3/<package>` | `symbian/` (filled by `fetch.sh`) |
| `PHOREPOC_BUILD` | every generated file: `epoc32/`, `casefix/`, `x32img/`, `arm/`, `rsc/` ... | `build/` |
| `PHOREPOC_OS_ROOT` | our overlay and components: `toolchain/{shims,overlay,kinc,generated}`, `src/`, the board and port files | `os/` |

`PHOREPOC_OS_ROOT` is temporary: until the overlay and our components move into `os/`, it has to point at a checkout
that still holds them. The registry itself is complete; entries whose files are not there cannot be built yet.

## Building

From the pdk checkout:

```sh
go build -o ph ./cmd/ph
./ph fetch                  # Symbian source -> core/symbian/
./ph export                 # epoc32/include tree
./ph casefix                # case-insensitive include links
./ph build palette estor    # x32 images -> core/build/x32img/
./ph arm palette estor      # ARM images
./ph help
```

Every command runs in the toolchain container (or directly when already inside one). Build output is deterministic:
the same inputs give the same bytes, which `ph tests` checks against another build.

## Licence

Eclipse Public License 1.0 (see `LICENSE`).
