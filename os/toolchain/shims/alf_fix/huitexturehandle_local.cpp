// Copyright The Phorepoc Authors
// SPDX-License-Identifier: Apache-2.0

// huitexturehandle_local.cpp -- OURS (not Symbian source): a private copy of THuiTextureHandle (the REAL
// coretoolkit/src/HuiTextureHandle.cpp, unmodified) compiled into the clients of hitchcock.dll that copy the class.
//
// THuiTextureHandle is NONSHARABLE_CLASS with virtual functions (MHuiTexture + MHuiSegmentedTexture). RVCT emits the
// class's vtable weakly in every TU that needs it; g++ emits it only where the key function is defined (hitchcock.dll),
// and the frozen hitchcocku.DEF exports no vtable. alfappservercore.dll and huirenderpluginvg.dll (inline copy
// constructor of the handle) then reference `vtable for THuiTextureHandle` -> a loud unimpl stub. Defining the class's
// (trivial: a name, two sizes) members here gives each client its own, consistent vtable.
#include <symbian-2010/oss.FCL.sf.mw.uiaccelerator/uiacceltk/hitchcock/coretoolkit/src/HuiTextureHandle.cpp>
