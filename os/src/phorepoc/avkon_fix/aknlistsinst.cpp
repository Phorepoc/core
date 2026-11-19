// Copyright The Phorepoc Authors
// SPDX-License-Identifier: EPL-2.0 OR GPL-2.0-or-later

// OURS: the three AknListBoxLinesTemplate<T> constructors the release's avkon exports (AVKONU.DEF ordinals 2010/2011,
// 2013/2014, 2016/2017) and that nothing in avkon's own sources instantiates. RVCT emits every EXPORT_C member of a
// template; GCC only the ones a translation unit uses, so these ordinals pointed at avkon's unimplemented-export
// stub and an application that builds a list from them (the Menu in list view: CMmListBox derives from
// AknListBoxLinesTemplate<CEikFormattedCellListBox>) was killed with EUSRNIMP at the call (2026-11-19). The body is the
// release's (aknlists.cpp: iResized = EFalse); only the constructors are instantiated here.
#include <aknlists.h>

template<class T>
EXPORT_C AknListBoxLinesTemplate<T>::AknListBoxLinesTemplate(TInt /*aResourceId*/)
    {
    iResized = EFalse;
    }

template EXPORT_C AknListBoxLinesTemplate<CAknColumnListBox>::AknListBoxLinesTemplate(TInt);
template EXPORT_C AknListBoxLinesTemplate<CEikSettingsListBox>::AknListBoxLinesTemplate(TInt);
template EXPORT_C AknListBoxLinesTemplate<CEikFormattedCellListBox>::AknListBoxLinesTemplate(TInt);

// The vtable of AknPopupListEmpty<CEikFormattedCellListBox> (ordinal 3670; its Draw and type info were already exported):
// the template is only ever a base of the popup list boxes, GCC emitted the vtable (COMDAT) nowhere that avkon exports it from.
template class AknPopupListEmpty<CEikFormattedCellListBox>;
