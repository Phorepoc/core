// Copyright The Phorepoc Authors
// SPDX-License-Identifier: Apache-2.0

// pb_dummy.cpp -- the dummy web engine of the browser frontend: a PheWebEngineApi table (phorepoc/phewebengine.h) that
// "loads" any URL in a few timer ticks (progress, title, security from the scheme), keeps a real back/forward history per
// view and draws a placeholder page into a BGRA buffer (colour from the URL, text-line stripes, scrollable). It lets the UI
// run without WebKit or NetSurf; a real engine implements the same table in its own repo.
#include <e32base.h>
#include <e32std.h>
#include "pb_engines.h"

namespace
	{
	const TInt KMaxViews = 16;
	const TInt KHist = 12;
	const TInt KEvQ = 64;
	const TInt KContentScreens = 4;

	struct TView
		{
		TInt iId;
		TInt iW, iH;
		TUint32* iPx;
		TBool iVisible;
		TBuf<256> iHist[KHist];
		TInt iPos, iCount;
		TBuf<96> iTitle;
		TInt iProgress;
		TBool iLoading;
		TInt iSecurity;
		TInt iScrollY;
		TInt iFind;
		};

	TView* gViews[KMaxViews];
	PheEvent gQ[KEvQ];
	TInt gQHead = 0, gQTail = 0;
	PheNotify gNotify = NULL;
	void* gCtx = NULL;
	CPeriodic* gTimer = NULL;
	TInt gNextId = 1;

	TView* Find(PheView aId)
		{
		for (TInt i = 0; i < KMaxViews; ++i)
			if (gViews[i] && gViews[i]->iId == aId)
				return gViews[i];
		return NULL;
		}

	void Post(PheView aV, TInt aKind, TInt aA = 0, TInt aB = 0)
		{
		TInt next = (gQTail + 1) % KEvQ;
		if (next == gQHead)
			return;                              // full: drop (the UI also polls state)
		PheEvent& e = gQ[gQTail];
		e.kind = aKind; e.view = aV; e.a = aA; e.b = aB; e.id = 0;
		gQTail = next;
		if (gNotify)
			gNotify(gCtx, aV);
		}

	TUint32 Hash(const TDesC& aS)
		{
		TUint32 h = 2166136261u;
		for (TInt i = 0; i < aS.Length(); ++i)
			h = (h ^ aS[i]) * 16777619u;
		return h;
		}

	TUint32 Mix(TUint32 aC, TInt aPercent)         // 0xAARRGGBB scaled toward white (>100) or black (<100)
		{
		TInt r = (aC >> 16) & 0xFF, g = (aC >> 8) & 0xFF, b = aC & 0xFF;
		r = Min(255, r * aPercent / 100); g = Min(255, g * aPercent / 100); b = Min(255, b * aPercent / 100);
		return 0xFF000000u | (r << 16) | (g << 8) | b;
		}

	void Render(TView& v)
		{
		if (!v.iPx || v.iW <= 0 || v.iH <= 0)
			return;
		const TUint32 base = 0xFF000000u | (Hash(v.iHist[v.iPos]) & 0x00B0B0B0u) | 0x00404040u;
		const TUint32 bg = 0xFFFFFFFFu, line = Mix(base, 130), head = base, block = Mix(base, 160);
		for (TInt y = 0; y < v.iH; ++y)
			{
			const TInt yy = y + v.iScrollY;
			const TInt band = yy / 160, off = yy % 160;
			TUint32* row = v.iPx + y * v.iW;
			TUint32 c = bg;
			TInt xEnd = v.iW;
			if (yy < 56)
				c = head;                        // page header band
			else if (off >= 112)
				c = bg;
			else if (band % 3 == 1 && off < 100)
				c = block;                       // an image-like block
			else if ((off % 14) < 8)
				{
				c = line;                        // a text line, ragged right
				xEnd = 16 + (v.iW - 32) * (55 + ((band * 37 + off / 14 * 23) % 45)) / 100;
				}
			for (TInt x = 0; x < v.iW; ++x)
				row[x] = (c == bg || c == head || c == block || (x >= 16 && x < xEnd)) ? c : bg;
			}
		if (v.iFind)                             // find-in-page demo: a yellow highlight on the first lines
			for (TInt y = 70; y < Min(v.iH, 78); ++y)
				for (TInt x = 16; x < Min(v.iW, 90); ++x)
					v.iPx[y * v.iW + x] = 0xFFFFE000u;
		}

	void Commit(TView& v)
		{
		const TDesC& u = v.iHist[v.iPos];
		v.iSecurity = u.Left(8).CompareF(_L("https://")) == 0 ? PHE_SEC_SECURE : PHE_SEC_NONE;
		TInt s = u.Locate(':');
		TPtrC host = (s >= 0 && u.Length() > s + 3) ? u.Mid(s + 3) : u;
		TInt e = host.Locate('/');
		if (e >= 0)
			host.Set(host.Left(e));
		v.iTitle.Copy(host.Left(60));
		v.iTitle.Append(_L(" (dummy engine)"));
		v.iScrollY = 0;
		}

	TInt Tick(TAny*)
		{
		TBool any = EFalse;
		for (TInt i = 0; i < KMaxViews; ++i)
			{
			TView* v = gViews[i];
			if (!v || !v->iLoading)
				continue;
			any = ETrue;
			v->iProgress += 12;
			if (v->iProgress >= 36 && v->iProgress < 48)
				{
				Commit(*v);
				Post(v->iId, PHE_EV_COMMIT, 0, v->iSecurity);
				Post(v->iId, PHE_EV_TITLE);
				}
			if (v->iProgress >= 100)
				{
				v->iProgress = 100;
				v->iLoading = EFalse;
				Render(*v);
				Post(v->iId, PHE_EV_PROGRESS, 100);
				Post(v->iId, PHE_EV_REPAINT);
				Post(v->iId, PHE_EV_FINISHED);
				}
			else
				Post(v->iId, PHE_EV_PROGRESS, v->iProgress);
			}
		(void)any;
		return 1;
		}

	// ---- the table ----------------------------------------------------------------------------------------------------
	int Init(PheNotify aN, void* aC)
		{
		gNotify = aN; gCtx = aC;
		if (!gTimer)
			{
			gTimer = CPeriodic::New(CActive::EPriorityLow);
			if (!gTimer)
				return KErrNoMemory;
			gTimer->Start(100000, 100000, TCallBack(Tick, NULL));   // 10 Hz
			}
		return 0;
		}

	void Shutdown()
		{
		delete gTimer; gTimer = NULL;
		for (TInt i = 0; i < KMaxViews; ++i)
			if (gViews[i]) { User::Free(gViews[i]->iPx); delete gViews[i]; gViews[i] = NULL; }
		gNotify = NULL;
		}

	void SetUA(const unsigned short*, int) {}
	void SetIap(const char*) {}
	void MemPressure(int aLevel)
		{
		if (aLevel >= PHE_MEM_LOW)
			for (TInt i = 0; i < KMaxViews; ++i)
				if (gViews[i] && !gViews[i]->iVisible) { User::Free(gViews[i]->iPx); gViews[i]->iPx = NULL; }
		}
	PheView NewId() { return gNextId++; }

	void Create(PheView aV, int aW, int aH, int, const unsigned char*, int)
		{
		for (TInt i = 0; i < KMaxViews; ++i)
			if (!gViews[i])
				{
				TView* v = new TView;
				if (!v)
					return;
				v->iId = aV; v->iW = aW; v->iH = aH; v->iVisible = ETrue;
				v->iPx = (TUint32*)User::Alloc(aW * aH * 4);
				v->iPos = -1; v->iCount = 0; v->iProgress = 100; v->iLoading = EFalse; v->iSecurity = 0; v->iScrollY = 0; v->iFind = 0;
				gViews[i] = v;
				return;
				}
		}

	void Destroy(PheView aV)
		{
		for (TInt i = 0; i < KMaxViews; ++i)
			if (gViews[i] && gViews[i]->iId == aV)
				{
				User::Free(gViews[i]->iPx); delete gViews[i]; gViews[i] = NULL;
				}
		}

	void Resize(PheView aV, int aW, int aH)
		{
		TView* v = Find(aV);
		if (!v || (aW == v->iW && aH == v->iH))
			return;
		User::Free(v->iPx);
		v->iW = aW; v->iH = aH;
		v->iPx = (TUint32*)User::Alloc(aW * aH * 4);
		if (v->iPos >= 0 && !v->iLoading)
			{ Render(*v); Post(aV, PHE_EV_REPAINT); }
		}

	void Visible(PheView aV, int aVis)
		{
		TView* v = Find(aV);
		if (!v)
			return;
		v->iVisible = aVis;
		if (aVis && !v->iPx && v->iW > 0)
			{
			v->iPx = (TUint32*)User::Alloc(v->iW * v->iH * 4);
			if (v->iPos >= 0) { Render(*v); Post(aV, PHE_EV_REPAINT); }
			}
		}

	void Suspend(PheView aV)
		{
		TView* v = Find(aV);
		if (v) { User::Free(v->iPx); v->iPx = NULL; }
		}

	int SaveState(PheView aV, unsigned char* aBuf, int aCap)
		{
		TView* v = Find(aV);
		if (!v || v->iPos < 0)
			return 0;
		const TInt n = v->iHist[v->iPos].Size();
		if (aBuf && aCap >= n)
			Mem::Copy(aBuf, v->iHist[v->iPos].Ptr(), n);   // the blob is just the current URL
		return n;
		}

	void StartLoad(TView& v)
		{
		v.iLoading = ETrue; v.iProgress = 0;
		Post(v.iId, PHE_EV_START, 0, 0);
		Post(v.iId, PHE_EV_HISTORY);
		}

	void Load(PheView aV, const unsigned short* aUrl, int aLen)
		{
		TView* v = Find(aV);
		if (!v)
			return;
		if (v->iPos == KHist - 1)                // shift the oldest out
			{
			for (TInt i = 1; i < KHist; ++i) v->iHist[i - 1] = v->iHist[i];
			--v->iPos;
			}
		++v->iPos;
		v->iCount = v->iPos + 1;
		v->iHist[v->iPos].Copy(TPtrC16(aUrl, Min(aLen, 255)));
		StartLoad(*v);
		}

	void Cmd(PheView aV, int aCmd)
		{
		TView* v = Find(aV);
		if (!v)
			return;
		switch (aCmd)
			{
		case PHE_CMD_STOP:
			if (v->iLoading) { v->iLoading = EFalse; Post(aV, PHE_EV_FINISHED); }
			break;
		case PHE_CMD_RELOAD:
		case PHE_CMD_RELOAD_BYPASS_CACHE:
			if (v->iPos >= 0) StartLoad(*v);
			break;
		case PHE_CMD_BACK:
			if (v->iPos > 0) { --v->iPos; StartLoad(*v); }
			break;
		case PHE_CMD_FORWARD:
			if (v->iPos + 1 < v->iCount) { ++v->iPos; StartLoad(*v); }
			break;
		default:
			break;
			}
		}

	void GetState(PheView aV, PheViewState* aOut)
		{
		Mem::FillZ(aOut, sizeof(*aOut));
		aOut->zoom_permille = 1000;
		TView* v = Find(aV);
		if (!v)
			return;
		aOut->can_back = v->iPos > 0;
		aOut->can_forward = v->iPos + 1 < v->iCount;
		aOut->loading = v->iLoading;
		aOut->progress = v->iProgress;
		aOut->security = v->iSecurity;
		aOut->scroll_y = v->iScrollY;
		aOut->content_w = v->iW;
		aOut->content_h = v->iH * KContentScreens;
		}

	int GetStr(PheView aV, int aWhich, unsigned short* aBuf, int aCap)
		{
		TView* v = Find(aV);
		if (!v)
			return 0;
		TPtrC src;
		switch (aWhich)
			{
		case PHE_STR_URL: if (v->iPos >= 0) src.Set(v->iHist[v->iPos]); break;
		case PHE_STR_TITLE: src.Set(v->iTitle); break;
		default: break;
			}
		if (aBuf && aCap > 0)
			Mem::Copy(aBuf, src.Ptr(), Min(aCap, src.Length()) * 2);
		return src.Length();
		}

	int Poll(PheView aV, PheEvent* aOut)
		{
		for (TInt i = gQHead; i != gQTail; i = (i + 1) % KEvQ)
			if (aV == 0 || gQ[i].view == aV)
				{
				*aOut = gQ[i];
				TInt j = i;
				while ((j + 1) % KEvQ != gQTail)         // close the gap
					{
					gQ[j] = gQ[(j + 1) % KEvQ];
					j = (j + 1) % KEvQ;
					}
				gQTail = (gQTail + KEvQ - 1) % KEvQ;
				return 1;
				}
		return 0;
		}

	void DialogReply(PheView, unsigned, int, const unsigned short*, int, const unsigned short*, int) {}

	void Scroll(PheView aV, int, int aDy)
		{
		TView* v = Find(aV);
		if (!v || v->iLoading)
			return;
		const TInt maxY = v->iH * (KContentScreens - 1);
		const TInt y = Max(0, Min(maxY, v->iScrollY + aDy));
		if (y != v->iScrollY)
			{
			v->iScrollY = y;
			Render(*v);
			Post(aV, PHE_EV_REPAINT);
			}
		}

	void Zoom(PheView, int, int, int) {}
	void Pointer(PheView, int, int, int) {}
	void Key(PheView, int, int, int) {}
	void Text(PheView, const unsigned short*, int) {}
	void Setting(PheView, int, int) {}

	int LockPixels(PheView aV, const unsigned int** aPx, int* aW, int* aH, int* aStride)
		{
		TView* v = Find(aV);
		if (!v || !v->iPx)
			return KErrNotFound;
		*aPx = v->iPx; *aW = v->iW; *aH = v->iH; *aStride = v->iW * 4;
		return 0;
		}
	void UnlockPixels(PheView) {}

	int Snapshot(PheView aV, unsigned int* aDst, int aW, int aH, int aStride)   // nearest-neighbour downscale
		{
		TView* v = Find(aV);
		if (!v || !v->iPx)
			return KErrNotFound;
		for (TInt y = 0; y < aH; ++y)
			{
			TUint32* d = (TUint32*)((TUint8*)aDst + y * aStride);
			const TUint32* s = v->iPx + (y * v->iH / aH) * v->iW;
			for (TInt x = 0; x < aW; ++x)
				d[x] = s[x * v->iW / aW];
			}
		return 0;
		}

	void FindText(PheView aV, const unsigned short*, int aLen, int)
		{
		TView* v = Find(aV);
		if (!v)
			return;
		v->iFind = aLen > 0;
		Render(*v);
		Post(aV, PHE_EV_FIND_RESULT, aLen > 0 ? 3 : 0, aLen > 0 ? 1 : 0);
		Post(aV, PHE_EV_REPAINT);
		}
	void FindNext(PheView, int) {}
	void FindClose(PheView aV)
		{
		TView* v = Find(aV);
		if (v) { v->iFind = 0; Render(*v); Post(aV, PHE_EV_REPAINT); }
		}
	void DlAccept(PheView, unsigned, const unsigned short*, int) {}
	void DlCancel(PheView, unsigned) {}
	void ClearData(int) {}

	const PheWebEngineApi KApi =
		{
		sizeof(PheWebEngineApi), PHE_WEBENGINE_VERSION, "Dummy", "0",
		PHE_CAP_FIND | PHE_CAP_SNAPSHOT | PHE_CAP_PRIVATE | PHE_CAP_SUSPEND | PHE_CAP_STATE | PHE_CAP_REFLOW, 0,
		Init, Shutdown, SetUA, SetIap, MemPressure,
		NewId, Create, Destroy, Resize, Visible, Suspend, SaveState,
		Load, Cmd, GetState, GetStr, Poll, DialogReply,
		Scroll, Zoom, Pointer, Key, Text, Setting,
		LockPixels, UnlockPixels, Snapshot,
		FindText, FindNext, FindClose,
		DlAccept, DlCancel,
		ClearData
		};
	}

extern "C" const PheWebEngineApi* PheDummyGetApi(unsigned aWantedVersion)
	{
	return aWantedVersion == PHE_WEBENGINE_VERSION ? &KApi : NULL;
	}
