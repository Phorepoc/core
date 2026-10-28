// Copyright The Phorepoc Authors
// SPDX-License-Identifier: Apache-2.0

// The advanced audio controller's frame table (mmaudio_plat frametable_api FrameTable.h), in place of the release's
// frametable_stub (every call answered "nothing": no seeking, no position events in MP3/AAC/AMR playback).
// The decoder submits the byte position of every frame it decodes (SubmitTableEntry, relative to the last
// SetSourceReference); the table maps time <-> byte position for seeking, and fires the position / play-window-end /
// decode-interval events. Frame n starts at n * samplesPerFrame / sampleRate seconds. Long files keep every frame
// until KMaxEntries, then every other one (ShrinkTable).
#include <FrameTable.h>

const TInt KMaxEntries = 8192;
const TUint KNone = KMaxTUint;

CFrameTable::CFrameTable()
    {
    }

void CFrameTable::ConstructL()
    {
    ResetTable();
    iPosEventMs = iPlayWindowEndPosEventMs = KNone;
    }

EXPORT_C CFrameTable* CFrameTable::NewL()
    {
    CFrameTable* self = new (ELeave) CFrameTable;
    CleanupStack::PushL(self);
    self->ConstructL();
    CleanupStack::Pop(self);
    return self;
    }

EXPORT_C CFrameTable::~CFrameTable()
    {
    iFrameTable.Close();
    iLowResFrameTable.Close();
    }

static TUint FrameTimeMs(TUint aFrame, TUint aSamplesPerFrame, TUint aSampleRate)
    {
    return aSampleRate ? (TUint)((TUint64)aFrame * aSamplesPerFrame * 1000 / aSampleRate) : 0;
    }

EXPORT_C TInt CFrameTable::InitFrameTable(TInt aSampleRate, TInt aSamplesPerFrame)
    {
    if (aSampleRate <= 0 || aSamplesPerFrame <= 0)
        return KErrArgument;
    iSampleRate = aSampleRate;
    iSamplesPerFrame = aSamplesPerFrame;
    iMsecPerFrame = FrameTimeMs(1, iSamplesPerFrame, iSampleRate);
    return KErrNone;
    }

TUint CFrameTable::CalcFrameFromTimeMs(TUint aTimeMs)
    {
    return iSamplesPerFrame ? (TUint)((TUint64)aTimeMs * iSampleRate / 1000 / iSamplesPerFrame) : 0;
    }

EXPORT_C TInt CFrameTable::SubmitTableEntry(TUint aPos)
    {
    if (!iSampleRate)
        return KErrNotReady;
    TBufRefPosEntry e;
    e.iPos = aPos + iOffset;
    e.iFrameNum = iFrameNum + iCurrentFrameCount++;
    const TInt n = iFrameTable.Count();
    if (n == 0 || e.iFrameNum > iFrameTable[n - 1].iFrameNum)
        {	// only frames past the end extend the table (a backward seek replays known ones)
        if (iBufCnt == 0 || (e.iFrameNum % (iBufCnt + 1)) == 0)
            {
            const TInt err = iFrameTable.Append(e);
            if (err != KErrNone)
                return err;
            }
        if (e.iPos > iLastBytePos)
            iLastBytePos = e.iPos;
        const TUint ms = FrameTimeMs(e.iFrameNum, iSamplesPerFrame, iSampleRate);
        if (ms)
            iAvgBitrate = (TUint)((TUint64)e.iPos * 8 * 1000 / ms);
        iOkToShrink = iFrameTable.Count() > KMaxEntries;
        }
    const TUint nowMs = FrameTimeMs(e.iFrameNum, iSamplesPerFrame, iSampleRate);
    if (!iObserver)
        return KErrNone;
    if (iPosEventMs != KNone && nowMs >= iPosEventMs)
        {
        iPosEventMs = KNone;
        iEventPos = e.iPos;
        iObserver->HandleFrameTableEvent(EPosReached);
        }
    if (iPlayWindowEndPosEventMs != KNone && nowMs >= iPlayWindowEndPosEventMs)
        {
        iPlayWindowEndPosEventMs = KNone;
        iEventPos = e.iPos;
        iObserver->HandleFrameTableEvent(EPlayWindowEndPosReached);
        }
    if (iIntervalEvent && nowMs >= (TUint)iIntervalRef + iIntervalEvent)
        {
        iIntervalRef = nowMs;
        iObserver->HandleFrameTableEvent(EDecodeInterval);
        }
    return KErrNone;
    }

TInt CFrameTable::SubmitLowResTableEntry(TBufRefPosEntry& aEntry)
    {
    return iLowResFrameTable.Append(aEntry);
    }

EXPORT_C TInt CFrameTable::FindFramePosFromTime(TUint& aTimeMs, TUint& aPos)
    {
    const TInt n = iFrameTable.Count();
    if (n == 0)
        return KErrNotFound;
    const TUint frame = CalcFrameFromTimeMs(aTimeMs);
    if (frame > iFrameTable[n - 1].iFrameNum)
        return KErrNotFound;			// not decoded yet: the controller reads on
    TInt lo = 0, hi = n - 1;			// the last entry at or before the frame
    while (lo < hi)
        {
        const TInt mid = (lo + hi + 1) / 2;
        if (iFrameTable[mid].iFrameNum <= frame)
            lo = mid;
        else
            hi = mid - 1;
        }
    aPos = iFrameTable[lo].iPos;
    aTimeMs = FrameTimeMs(iFrameTable[lo].iFrameNum, iSamplesPerFrame, iSampleRate);
    return KErrNone;
    }

EXPORT_C TInt CFrameTable::FindFrameTimeFromPos(TUint& aTimeMs, TUint& aPos)
    {
    const TInt n = iFrameTable.Count();
    if (n == 0 || aPos < iFrameTable[0].iPos || aPos > iFrameTable[n - 1].iPos)
        return KErrNotFound;
    TInt lo = 0, hi = n - 1;
    while (lo < hi)
        {
        const TInt mid = (lo + hi + 1) / 2;
        if (iFrameTable[mid].iPos <= aPos)
            lo = mid;
        else
            hi = mid - 1;
        }
    aPos = iFrameTable[lo].iPos;
    aTimeMs = FrameTimeMs(iFrameTable[lo].iFrameNum, iSamplesPerFrame, iSampleRate);
    return KErrNone;
    }

EXPORT_C TInt CFrameTable::LastFramePos(TUint& aPos)
    {
    const TInt n = iFrameTable.Count();
    if (n == 0)
        return KErrNotFound;
    aPos = iFrameTable[n - 1].iPos;
    return KErrNone;
    }

EXPORT_C TInt CFrameTable::LastFrameTime(TUint& aTimeMs)
    {
    const TInt n = iFrameTable.Count();
    if (n == 0)
        return KErrNotFound;
    aTimeMs = FrameTimeMs(iFrameTable[n - 1].iFrameNum, iSamplesPerFrame, iSampleRate);
    return KErrNone;
    }

EXPORT_C void CFrameTable::ResetTable()
    {
    iFrameTable.Reset();
    iLowResFrameTable.Reset();
    iLastBytePos = iAvgBitrate = iOffset = iTimeRefMs = iBufCnt = 0;
    iFrameNum = iCurrentFrameCount = 0;
    iIntervalRef = 0;
    iEventPos = 0;
    iOkToShrink = EFalse;
    }

// halve the table (every other entry) once it is past KMaxEntries; later frames are kept at the same spacing
EXPORT_C void CFrameTable::ShrinkTable()
    {
    if (!iOkToShrink)
        return;
    TInt w = 0;
    for (TInt r = 0; r < iFrameTable.Count(); r += 2)
        iFrameTable[w++] = iFrameTable[r];
    while (iFrameTable.Count() > w)
        iFrameTable.Remove(iFrameTable.Count() - 1);
    iFrameTable.Compress();
    iBufCnt = iBufCnt * 2 + 1;			// keep 1 in iBufCnt + 1 frames from now on
    iOkToShrink = EFalse;
    }

// the decoder restarts at aPos (bytes into the audio data), which is aTimeMs into the stream
EXPORT_C TInt CFrameTable::SetSourceReference(TUint aTimeMs, TUint aPos)
    {
    iTimeRefMs = aTimeMs;
    iOffset = aPos;
    iFrameNum = CalcFrameFromTimeMs(aTimeMs);
    iCurrentFrameCount = 0;
    iIntervalRef = aTimeMs;
    return KErrNone;
    }

EXPORT_C TInt CFrameTable::RegisterForEvent(TFrameTableEvent aEvent, MFrameTableEventObserver* aObserver, TUint aParam)
    {
    iObserver = aObserver;
    switch (aEvent)
        {
        case EPosReached:
            iPosEventMs = aParam;
            return KErrNone;
        case EPlayWindowEndPosReached:
            iPlayWindowEndPosEventMs = aParam;
            return KErrNone;
        case EDecodeInterval:
            iIntervalEvent = aParam;
            iIntervalRef = FrameTimeMs(iFrameNum + iCurrentFrameCount, iSamplesPerFrame, iSampleRate);
            return KErrNone;
        }
    return KErrArgument;
    }

EXPORT_C TInt CFrameTable::UnRegisterForEvent(TFrameTableEvent aEvent, MFrameTableEventObserver* /*aObserver*/)
    {
    switch (aEvent)
        {
        case EPosReached:
            iPosEventMs = KNone;
            return KErrNone;
        case EPlayWindowEndPosReached:
            iPlayWindowEndPosEventMs = KNone;
            return KErrNone;
        case EDecodeInterval:
            iIntervalEvent = 0;
            return KErrNone;
        }
    return KErrArgument;
    }

EXPORT_C TInt CFrameTable::Bitrate()
    {
    return iAvgBitrate ? (TInt)iAvgBitrate : KErrNotReady;
    }

EXPORT_C TInt CFrameTable::IsSeeking(TBool& aIsSeeking)
    {
    aIsSeeking = iPosEventMs != KNone;
    return KErrNone;
    }

EXPORT_C TInt CFrameTable::GetLastPosEvent(TUint& aPos)
    {
    aPos = iEventPos;
    return KErrNone;
    }
