// Copyright The Phorepoc Authors
// SPDX-License-Identifier: EPL-2.0 OR GPL-2.0-or-later

// phtrace_wfc.cpp -- OURS: exported wfc calls as traced pass-throughs (orientation work, 2026-10-12).
// libwfc's own wfcapi.c is compiled with -D<name>=ph_real_<name> (emitter/components.py); this file defines the
// exported names. A WFC_CONTEXT_ROTATION change -- the window server's display render stage rotating the screen for a
// landscape mode -- is logged with the "PH:" prefix, which the kernel seam forwards to PhoneLab. After a change to a
// non-zero rotation the next element rectangles are logged too (the rotated frame is black: where do the elements go?).
#include <e32debug.h>
#undef wfcSetContextAttribi
#undef wfcSetElementAttribiv
#undef wfcSetElementAttribfv
#include <WF/wfc.h>

extern "C" void ph_real_wfcSetContextAttribi(WFCDevice dev, WFCContext ctx, WFCContextAttrib attrib, WFCint value);
extern "C" void ph_real_wfcSetElementAttribiv(WFCDevice dev, WFCElement element, WFCElementAttrib attrib, WFCint count,
	const WFCint* values);
extern "C" void ph_real_wfcSetElementAttribfv(WFCDevice dev, WFCElement element, WFCElementAttrib attrib, WFCint count,
	const WFCfloat* values);

static TInt PhRectBudget = 0;		// element rectangles still to log (armed by a rotation change)

extern "C" EXPORT_C void wfcSetContextAttribi(WFCDevice dev, WFCContext ctx, WFCContextAttrib attrib, WFCint value)
	{
	if (attrib == WFC_CONTEXT_ROTATION)
		{
		RDebug::Printf("PH: wfc context %d rotation -> %d", (TInt)ctx, (TInt)value);
		PhRectBudget = 40;
		}
	ph_real_wfcSetContextAttribi(dev, ctx, attrib, value);
	}

static const char* PhRectName(WFCElementAttrib aAttrib)
	{
	return aAttrib == WFC_ELEMENT_DESTINATION_RECTANGLE ? "dst" : aAttrib == WFC_ELEMENT_SOURCE_RECTANGLE ? "src" : NULL;
	}

extern "C" EXPORT_C void wfcSetElementAttribiv(WFCDevice dev, WFCElement element, WFCElementAttrib attrib, WFCint count,
	const WFCint* values)
	{
	const char* n = PhRectName(attrib);
	if (n && PhRectBudget > 0 && values && count == 4)
		{
		--PhRectBudget;
		RDebug::Printf("PH: wfc element %d %s %d,%d %dx%d", (TInt)element, n, values[0], values[1], values[2], values[3]);
		}
	ph_real_wfcSetElementAttribiv(dev, element, attrib, count, values);
	}

extern "C" EXPORT_C void wfcSetElementAttribfv(WFCDevice dev, WFCElement element, WFCElementAttrib attrib, WFCint count,
	const WFCfloat* values)
	{
	const char* n = PhRectName(attrib);
	if (n && PhRectBudget > 0 && values && count == 4)
		{
		--PhRectBudget;
		RDebug::Printf("PH: wfc element %d %s(f) %d,%d %dx%d", (TInt)element, n, (TInt)values[0], (TInt)values[1],
			(TInt)values[2], (TInt)values[3]);
		}
	ph_real_wfcSetElementAttribfv(dev, element, attrib, count, values);
	}
