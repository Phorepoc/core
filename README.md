# core

This is the Phorepoc operating system. It holds everything that's the same on every device: our changes to the
Symbian source, the components we've written ourselves, and the list of every Symbian component together with how to
build it. The Symbian source itself isn't stored here. `fetch.sh` downloads it at the exact commits listed in
`sources.lock`.

The tools that turn all this into a bootable image are in the [pdk](https://github.com/Phorepoc/pdk), and you drive them
with its `ph` command.

## Devices

Core knows no devices. It builds for an architecture (`x32`, `arm`), and holds the interfaces hardware plugs into: the
Phorepoc HAL functions (`os/src/phorepoc/inc/phhal.h`), the GPU channel (`os/src/symgl/symgpu.h`), the video plane, and
null drivers for all of them. Default settings are in `os/config/`.

A device is a board repository on top of core: it names a SoC repository with the kernel port and drivers, the
optional layers it ships, and how its images are made. How the layers fit together, and how to write your own board,
is in the pdk's [docs/layering.md](https://github.com/Phorepoc/pdk/blob/main/docs/layering.md).

## Building

You'll need Docker and a checkout of the pdk next to this one, plus the repositories of the board you build for
(`ph board list` shows the boards it finds). For a board called `myboard`:

```sh
ph source fetch                        # download the Symbian source into core/symbian/
ph source export -a                    # put together the include tree
ph source mbg -a                       # generate the icon headers
ph component resources -A -b myboard        # compile the resource files
ph component build -A -b myboard            # build every component
ph image rom -b myboard                     # and put them into a ROM
```

`-b` tells `ph` which board to build for. Without it, `ph` builds core alone for an architecture (`--arch x32` or
`--arch arm`). Everything runs inside the
toolchain container, so you don't need any Symbian tools installed. Builds are reproducible: the same source gives the
same bytes, and `ph image compare` checks one build against another.

The [ph manual](https://github.com/Phorepoc/pdk/tree/main/docs/ph) explains each command in detail.

## Finding your way around

```
os/src/                  components we wrote (pheshell, phetty, phgl, phvg and more), with their tests
os/toolchain/shims/      our replacement copies of Symbian source files, one folder per change
os/toolchain/            other headers and runtime pieces every image is built with
os/shim/                 stand-in headers for interfaces the 2010 release left out
os/config/               the default settings every device starts from
os/toolchain/x32/        the x32 user-side runtime (startup, C++ runtime glue, setjmp)
os/tools/                build recipes for third-party code
registry/components.py   one entry for each component, saying how it's built from its .mmp
registry/rom_heap.tsv    heap and stack sizes for the programs in the ROM
sources.lock, fetch.sh   which Symbian source we build, and the script that downloads it
```

By default `ph` expects this repository in a folder called `core` next to the pdk. If yours is somewhere else, point
`PHOREPOC_CORE` at it. `PHOREPOC_SYMBIAN` and `PHOREPOC_BUILD` do the same for the Symbian source and the build
output, which otherwise go into `symbian/` and `build/` here.

## License

Code we wrote is Apache-2.0. Symbian files we've changed are EPL-2.0, with GPL-2.0-or-later as a Secondary License
(EPL-1.0 lets us move them to the newer version). Symbian sources we use unchanged are downloaded from upstream and stay
EPL-1.0. A few third-party files carry their own license in their headers. The license texts are in `LICENSES/`.

Contributions are welcome under the same terms. Add your name to `AUTHORS` in your first pull request.
