# core

This is the Phorepoc operating system. It holds everything that's the same on every device: our changes to the
Symbian source, the components we've written ourselves, and the list of every Symbian component together with how to
build it. The Symbian source itself isn't stored here. `fetch.sh` downloads it at the exact commits listed in
`sources.lock`.

The tools that turn all this into a bootable image are in the [pdk](https://github.com/Phorepoc/pdk), and you drive them
with its `ph` command.

## Devices

Anything specific to a piece of hardware lives in an adaptation repository, which gets built together with core:

- [adaptation-x86-64](https://github.com/Phorepoc/adaptation-x86-64) is the kernel port to 64-bit PCs.
- [adaptation-pc](https://github.com/Phorepoc/adaptation-pc) covers QEMU, VirtualBox and real PCs.
- [adaptation-a64](https://github.com/Phorepoc/adaptation-a64) has the drivers for the Allwinner A64 chip.
- [adaptation-pinephone](https://github.com/Phorepoc/adaptation-pinephone) is the PinePhone itself.

A few larger pieces have repositories of their own. [tls](https://github.com/Phorepoc/tls) replaces Symbian's TLS with
mbedTLS, and [mesa-symbian](https://github.com/Phorepoc/mesa-symbian) provides OpenGL ES. Video codecs come from
[codecpack](https://github.com/Phorepoc/codecpack), which is never built into an image. You install it from Get Apps.

## Building

You'll need Docker and a checkout of the pdk next to this one. To build the PC image:

```sh
ph source fetch                        # download the Symbian source into core/symbian/
ph source export -a                    # put together the include tree
ph source mbg -a                       # generate the icon headers
ph component resources -A -b pc        # compile the resource files
ph component build -A -b pc            # build every component
ph image rom -b pc                     # and put them into a ROM
```

`-b pc` tells `ph` which device to build for, and `ph board list` shows the others. Everything runs inside the
toolchain container, so you don't need any Symbian tools installed. Builds are reproducible: the same source gives the
same bytes, and `ph image compare` checks one build against another.

The [ph manual](https://github.com/Phorepoc/pdk/tree/main/docs/ph) explains each command in detail.

## Finding your way around

```
os/src/                  components we wrote (pheshell, phetty, phgl, phvg and more), with their tests
os/toolchain/shims/      our replacement copies of Symbian source files, one folder per change
os/toolchain/            other headers and runtime pieces every image is built with
os/shim/                 stand-in headers for interfaces the 2010 release left out
os/tools/                settings files the ROM is made from, and build recipes for third-party code
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
