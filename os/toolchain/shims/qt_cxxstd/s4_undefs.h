/* Copyright The Phorepoc Authors
 * SPDX-License-Identifier: EPL-2.0 OR GPL-2.0-or-later */
/* s4_undefs.h -- OURS (S4.2): force-included AFTER the variant .hrh for the Qt modules. The 2010 variant
 * (Symbian_OS_tb102sf.hrh) turns on Symbian^4 window-server features that the S^3 WSERV this board runs does not
 * have: Qt's effect-layer commands (ETfxCmdRestoreLayer ...). */
#undef SYMBIAN_GRAPHICS_WSERV_QT_EFFECTS
