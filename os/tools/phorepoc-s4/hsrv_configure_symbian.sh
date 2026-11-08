#!/usr/bin/env bash
# Copyright The Phorepoc Authors
# SPDX-License-Identifier: Apache-2.0
# hsrv_configure_symbian.sh -- Symbian^4 stage S4.5 (docs/phorepoc/symbian4-build.md): homescreensrv (the release's
# oss.FCL.sf.mw.homescreensrv) -- the content arsenal (contentstorage: the storage server, the app scanners, the Qt
# client API) and the platform API headers (homescreensrv_plat), qmake-generated in-source as the package's build.
#
#   bash tools/phorepoc-s4/hsrv_configure_symbian.sh        (host side; runs itself in Docker)
#
# Its .pro files use CONFIG += hb / mobility / service / ecomplugin: the feature files a Symbian SDK had installed in
# Qt's mkspecs (hb's configure, qtextensions' group/bld.inf exports) -> staged into .phorepoc-s4-build/qmake-features
# and found through QMAKEFEATURES.
#
# Out: .phorepoc-s4-build/hsrv-src (LF export, in-source build: bld.infs, mmps, moc output under its bin/tmp).
set -eu
. "$(dirname "${BASH_SOURCE[0]}")/s4env.sh"
s4_export oss.FCL.sf.mw.homescreensrv hsrv-src homescreensrv.pro

S4=$Q
SRC=$S4/hsrv-src
QMAKE=$S4/qt-symbian/bin/qmake
mkdir -p /epocroot
ln -sfn $B/epoc32 /epocroot/epoc32
export EPOCROOT=/epocroot/

# the SDK's installed qmake features
F=$S4/qmake-features
mkdir -p "$F"
cp -f "$S4/hb-src/hb.prf" "$S4/hb-symbian/hb_install.prf" "$S4/hb-src/mkspecs/docml2bin.prf" "$S4/hb-src/mkspecs/hb_functions.prf" "$F/"
cp -f "$S4/qtm-src/features/mobility.prf" "$S4/qtm-src/features/mobilityconfig.prf" "$F/"
cp -f "$S4/qte-src/qthighway/bin/service.prf" "$S4/qte-src/qtecomplugins/mkspecs/features/ecomplugin.prf" "$F/"
export QMAKEFEATURES=$F
echo "QT_BUILD_TREE = $S4/qt-symbian" > "$SRC/.qmake.cache"

# S4.7: + the activity framework, the task switcher (hsrunningappmonitor, its device dialog) and the app-key handler --
# homescreensrv.pro's Symbian SUBDIRS. PROS="<pro> ..." re-runs a subset (in-source: each qmake rewrites its tree)
PROS="${PROS:-homescreensrv_plat/homescreensrv_plat.pro contentstorage/contentstorage.pro activityfw/activityfw.pro taskswitcher/taskswitcher.pro hsappkeyhandler/hsappkeyhandler.pro}"
DIRS=$(for pro in $PROS; do d=$(dirname "$pro"); [ "$d" = homescreensrv_plat ] || echo "$SRC/$d"; done)
# the package-level inc/ the client exports land in (s4_exports.py, after this): qmake drops an INCLUDEPATH entry
# that does not exist yet (the task switcher dialog's ../../inc -> tstaskmonitor.h)
mkdir -p "$SRC/inc"
for pro in $PROS; do
  ( cd "$SRC/$(dirname "$pro")" && "$QMAKE" -cache "$SRC/.qmake.cache" -spec "$S4/qt-symbian/mkspecs/symbian-sbsv2" \
      -r "$SRC/$pro" > qmake.log 2>&1 ) || { echo "$pro: qmake FAILED"; tail -30 "$SRC/$(dirname "$pro")/qmake.log"; exit 1; }
  grep -i "error\|warning" "$SRC/$(dirname "$pro")/qmake.log" | grep -v "backslashes" | head -5 || true
done

# no C++ exceptions on this board (as Qt and Orbit): caclient's catch (std::bad_alloc) guards -> hb_noexcept.py
case " $PROS " in *contentstorage.pro*) python3 $HERE/hb_noexcept.py "$SRC/contentstorage/caclient" ;; esac
for d in activityfw taskswitcher; do
  case " $PROS " in *"$d/"*) python3 $HERE/hb_noexcept.py "$SRC/$d" ;; esac
done

# the generated sources (moc) the SBSv2 build makes before compiling, with the host tools
for mk in $(find $DIRS -name "Makefile*" -not -path "*/tsrc/*"); do
  d=$(dirname "$mk")
  ls "$d"/*.mmp >/dev/null 2>&1 && grep -q "^mocables:" "$mk" || continue
  sed -i -e 's#/epocroot//*epoc32//*bin/#'"$Q"'/qt-symbian/bin/#g' \
         -e 's#/epocroot/epoc32/include/Qt#'"$Q"'/qt-symbian/include/Qt#g' \
         -e 's#/epocroot/epoc32/mkspecs/#'"$Q"'/qt-src/mkspecs/#g' "$mk"
  # (Qt Mobility's module dirs are not Qt's: qtm_configure_symbian.sh's include/Qt<Module>)
  sed -i -E 's#'"$Q"'/qt-symbian/include/Qt(ServiceFramework|PublishSubscribe|SystemInfo|Mobility)#'"$Q"'/qtm-src/include/Qt\1#g' "$mk"
  # Orbit's docml -> binary layout compiler (the task switcher dialog's qrc names its output): the host build
  # docml2bin = the HOST converter built on the 4.7 host Qt (the 4.8 tree has no host QtGui; it writes Hb's binary docml, not Qt ABI)
  sed -i 's#/epocroot/epoc32/tools/docml2bin#LD_LIBRARY_PATH=$Q/qt-host/lib $Q/hb-host/docml2bin#g' "$mk"
  dm=; grep -q "^compiler_docml2bin_make_all:" "$mk" && dm=compiler_docml2bin_make_all
  ( cd "$d" && make -s -f "$(basename "$mk")" mocables $dm compiler_rcc_make_all > "mocables-$(basename "$mk").log" 2>&1 ) \
    || { echo "${mk#$SRC/}: mocables FAILED"; tail -3 "$d/mocables-$(basename "$mk").log"; }
done
for m in $(find $DIRS \( -name "*_dll.mmp" -o -name "*_exe.mmp" \) -not -path "*/tsrc/*"); do
  d=$(dirname "$m")
  for inc in $(grep -rhoE '#include "moc_[A-Za-z0-9_]+\.cpp"' --include=*.cpp "$d" | grep -oE 'moc_[A-Za-z0-9_]+\.cpp' | sort -u); do
    sed -i "/^SOURCE[[:space:]]*$inc\$/d" "$m"
  done
  echo "${m#$SRC/}: $(grep -c 'moc' "$m") moc lines"
done
