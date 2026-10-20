# arch.sh -- sourced by the web build scripts (IN-CONTAINER): ARCH=x32 (default, drun2.sh) or arm (the PinePhone, drun-arm.sh).
#   LTRIP  libstdc++'s target include dir name     CROSS  binutils prefix (ar/readelf/nm)     PROC  CMAKE_SYSTEM_PROCESSOR
#   ICU / DEPS / TC  the per-arch ICU install, dependency prefix and CMake toolchain file in the volume (/wv)
ARCH=${ARCH:-x32}
if [ "$ARCH" = arm ]; then LTRIP=arm-none-eabi; CROSS=arm-none-eabi-; PROC=arm; OSSL_TARGET=linux-armv4
  # the bare-metal compiler defines no __unix__: WTF/bmalloc would see no OS at all (x86_64-linux gcc has it, then -U__linux__)
  OSDEF="-D__unix__=1 -D__unix=1"
else LTRIP=x86_64-unknown-elf; CROSS=; PROC=x86_64; OSSL_TARGET=linux-x32; OSDEF=""; fi
ICU=/wv/icu-$ARCH/inst; DEPS=/wv/deps-$ARCH; TC=/wv/$ARCH-openc.cmake
NSO=/out/netsurf; [ "$ARCH" = arm ] && NSO=/out/netsurf-arm   # the NetSurf port's libpng/libnspsl/img-include
:   # (the last line above is a [ ] && test: a sourcing `set -e` script must not exit on its status)
