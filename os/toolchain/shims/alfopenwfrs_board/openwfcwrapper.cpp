// Copyright (c) 2008-2009 Nokia Corporation and/or its subsidiary(-ies).
// All rights reserved.
// This component and the accompanying materials are made available
// under the terms of the Eclipse Public License v2.0, available at
// https://www.eclipse.org/legal/epl-2.0/ (relicensed from v1.0 under
// section 7 of the Eclipse Public License v1.0).
// This Source Code may also be made available under the following Secondary
// Licenses when the conditions for such availability set forth in the Eclipse
// Public License, v. 2.0 are satisfied: GNU General Public License, version 2
// or later.
//
// Initial Contributors:
// Nokia Corporation - initial contribution.
//
// Contributors:
// The Phorepoc Authors
//
// Description: Owns, hides, and provides an interface to the OpenWFC compositor for use by the final (display) render stage.

#include "openwfcwrapper.h"
#include "panic.h"
#include "utils.h"

#include <WF/wfc.h>
#include "elementwrapper.h"
#include <graphics/compositionsurfaceupdate.h>
#include <graphics/symbianstream.h>
#include "openwfcpanics.h"
#include "openwfcjobmanager.h"

#include <graphics/eglsynchelper.h>

#define KRgbaBlack       0x000000FF

void Panic(TOpenWfcPanic aPanic)
    {
    _LIT(KPanic, "OpenWFC");
    User::Panic(KPanic, aPanic);
    }

class COpenWfcWrapper::OffScreenComposeGuard
    {
    public:
        OffScreenComposeGuard(TRequestStatus*& aRequestStatus);
        void SetOffScreenNativeStream(WFCNativeStreamType aNativeStream);
        void SetTargetNativeStream(WFCNativeStreamType aNativeStream);
        void SetSync(EGLSyncKHR aSync, EGLDisplay aDpy);
        void SetDeviceAndContext(WFCDevice aDevice, WFCContext aContext);
        void Close();
        void LogRequestStatusError(TInt aRequestStatusError);
    private:
        TRequestStatus*& iRequestStatus;
        EGLSyncKHR iSync;
        EGLDisplay iEGLDisplay;
        WFCDevice iDevice;
        WFCContext iContext;
        TInt    iRequestStatusError;
    };

COpenWfcWrapper::OffScreenComposeGuard::OffScreenComposeGuard(TRequestStatus*& aRequestStatus):
iRequestStatus(aRequestStatus),
iSync(EGL_NO_SYNC_KHR),
iDevice(WFC_INVALID_HANDLE),
iContext(WFC_INVALID_HANDLE),
iRequestStatusError(KErrNone)
    {}


void COpenWfcWrapper::OffScreenComposeGuard::SetSync(EGLSyncKHR aSync, EGLDisplay aDpy)
    {
    iSync = aSync;
    iEGLDisplay = aDpy;
    }

void COpenWfcWrapper::OffScreenComposeGuard::SetDeviceAndContext(WFCDevice aDevice, WFCContext aContext)
    {
    iContext = aContext; 
    iDevice = aDevice;
    }

void COpenWfcWrapper::OffScreenComposeGuard::LogRequestStatusError(TInt aRequestStatusError)
    {
    iRequestStatusError=aRequestStatusError;
    }

void COpenWfcWrapper::OffScreenComposeGuard::Close()
    {
    if (iContext != WFC_INVALID_HANDLE)
        {
        DestroyAllContextElements(iDevice, iContext);
        wfcDestroyContext(iDevice, iContext);
        }
  
    if (iSync != EGL_NO_SYNC_KHR)
        {
        eglDestroySyncKHR(iEGLDisplay, iSync);
        }

    if (iRequestStatus)
        {
        User::RequestComplete(iRequestStatus, iRequestStatusError);
        }
    }

TEMPLATE_SPECIALIZATION class RHashTableBase::Defaults<TSurfaceId, RHashTableBase::EDefaultSpecifier_Normal>
    {
public:
    inline static TGeneralHashFunction32 Hash();
    inline static TGeneralIdentityRelation Id();
    };

inline TGeneralHashFunction32 RHashTableBase::Defaults<TSurfaceId, RHashTableBase::EDefaultSpecifier_Normal>::Hash()
    {return (TGeneralHashFunction32)&DefaultHash::Integer;}

inline TGeneralIdentityRelation RHashTableBase::Defaults<TSurfaceId, RHashTableBase::EDefaultSpecifier_Normal>::Id()
    {return (TGeneralIdentityRelation)&DefaultIdentity::Integer;}

TUint32 COpenWfcWrapper::HashFunction(const TSurfaceId& aHashKey)
    {
    TPckgC<TSurfaceId> pckg(aHashKey);
    return DefaultHash::Des8(pckg);
    }
COpenWfcWrapper* COpenWfcWrapper::NewL(TInt aScreenNo, CDisplayPolicy* aDisplayPolicy)
	{
	COpenWfcWrapper* wrapper = new (ELeave) COpenWfcWrapper(aDisplayPolicy);
	CleanupStack::PushL(wrapper);
	wrapper->ConstructL(aScreenNo);
	CleanupStack::Pop(wrapper);
	return wrapper;
	}

COpenWfcWrapper::COpenWfcWrapper(CDisplayPolicy* aDisplayPolicy)
	: iUiSurface(TSurfaceId::CreateNullId()), iDisplayPolicy(aDisplayPolicy),
	iSourceMap(THashFunction32<TSurfaceId>(COpenWfcWrapper::HashFunction), TIdentityRelation<TSurfaceId>()),
	iDevice(WFC_INVALID_HANDLE),
	iOnScreenContext(WFC_INVALID_HANDLE),
	iScreenNumber(-1),
	iJobManager(NULL),
	iRotation(MWsScene::ESceneAntiClockwise0),
	iAutonomousCompositionInitiated(EFalse)
	{
	}

void COpenWfcWrapper::ConstructL(TInt aScreenId)
	{
    _LIT(KOpenWfcLog, "+ COpenWfcWrapper::ConstructL");
    RDebug::Print(KOpenWfcLog);

    iScreenNumber = aScreenId;
    WFCint filterList[] = { WFC_DEVICE_FILTER_SCREEN_NUMBER, iScreenNumber, WFC_NONE};

    iEGLDisplay = eglGetDisplay(EGL_DEFAULT_DISPLAY);
    eglInitialize(iEGLDisplay, NULL, NULL);
    RDebug::Printf("PH: alf: wrapper eglInitialize done (screen %d)", iScreenNumber);
        
    WFCint dev = 0;
    if ((wfcEnumerateDevices(&dev, 1, filterList) == 1) &&
        (dev != WFC_INVALID_HANDLE))
        {
        // clean previous errors
        wfcGetError(iDevice);
    
        // Let's get the device handle, opening in the same time the device
        iDevice = wfcCreateDevice(dev, NULL);
        RDebug::Printf("PH: alf: wrapper wfcCreateDevice -> %x", (TUint)iDevice);
        if (iDevice==NULL)
            {
            //Not enough information to deduce why the device could not be created. Report as memory failure
            User::Leave(KErrNoMemory);
            }
        //Can't clean previous errors until we have a device, and the errors should be none if it created successfully.
        OPENWFC_ASSERT_DEBUG(!wfcGetError(iDevice),EPanicWfcStartupErrorUnexpected);
        
        iOnScreenContext = wfcCreateOnScreenContext(iDevice, iScreenNumber, NULL);
        RDebug::Printf("PH: alf: wrapper wfcCreateOnScreenContext -> %x", (TUint)iOnScreenContext);
        if (iOnScreenContext==NULL)
            {
            TInt err = wfcGetError(iDevice);
            OPENWFC_ASSERT_DEBUG(err==WFC_ERROR_OUT_OF_MEMORY,EPanicWfcContextNotCreated);
            User::Leave((err==WFC_ERROR_OUT_OF_MEMORY)?KErrNoMemory:KErrUnknown);
            }

        wfcSetContextAttribi(iDevice, iOnScreenContext, WFC_CONTEXT_ROTATION, WFC_ROTATION_0);
        wfcSetContextAttribi(iDevice, iOnScreenContext, WFC_CONTEXT_BG_COLOR, KRgbaBlack);
        
        iContextDisplayControl = NULL;
        
        RDebug::Printf("PH: alf: wrapper creating the job manager (compositor thread)");
        User::LeaveIfNull(iJobManager = COpenWfcJobManger::NewL(*this, iDevice, iOnScreenContext, iScreenNumber));
        RDebug::Printf("PH: alf: wrapper job manager up");
        }
    else
        {
        // we cannot continue because we cannot find a device for the given screenId
        // Not enough information to get detailed error! Report as parameter not accepted.
        User::Leave(KErrArgument);
        }
    iSurfaceManager.Open();
    RDebug::Printf("PH: alf: wrapper ConstructL done");

    iSourceMap.Reserve(iInitialSourceMapSize);
    }

COpenWfcWrapper::~COpenWfcWrapper()
	{
	iUiElement1 = NULL;
	iUiElement2 = NULL;
    if(!iUiSurface.IsNull())
        UnregisterSurface(iUiSurface);
    
    if (iJobManager)
        {
        PauseComposition();
        delete iJobManager;
        }
    
    //destroy all the elements, which should remove all the element references.
    while (iCleanupElementList)
        {
        CElementWrapper* cleanupElement = iCleanupElementList;
        delete cleanupElement;
        OPENWFC_ASSERT_DEBUG(cleanupElement != iCleanupElementList,EPanicWfcElementNotRemovedOnShutdown);
        if (cleanupElement == iCleanupElementList)
            {
            break;  //can't keep cleaning up the same front item
            }
        }
    THashMapIter<TSurfaceId, SourceAndRef> iter(iSourceMap);
    const TSurfaceId* nextKey = iter.NextKey();
    while (nextKey)
        {
        const SourceAndRef* pSource = NULL;
        //destroy any remaining registered surfaces
        pSource = iter.CurrentValue();
        OPENWFC_ASSERT_DEBUG(pSource && pSource->elementRef == 0,EPanicWfcElementNotRemovedOnShutdown);
        wfcDestroySource(iDevice,pSource->source);
        nextKey = iter.NextKey();
        }

    iSourceMap.Close();
    iSurfaceManager.Close();
    eglTerminate(iEGLDisplay);

    // Destroying the context should take care of any sources or masks
    wfcDestroyContext(iDevice, iOnScreenContext);
    wfcDestroyDevice(iDevice);
	}

class CBaseExtension: public CBase
    {
    public:
        virtual TInt Extension_(TUint aExtensionId, TAny*& a0, TAny* a1)
            {   return CBase::Extension_(aExtensionId,a0,a1);   }
    };
TInt COpenWfcWrapper::Extension_(TUint aExtensionId, TAny*& aRetPtr, TAny* aExtra)
    {
#ifndef _DEBUG
    (void) aExtra;
#endif
    
    switch (aExtensionId)
        {
#ifdef _DEBUG
        case EExtensionDebugBackendGuid:
            return KErrNotSupported;
        case EExtensionDebugContextGuid:
            aRetPtr=(TAny*)OnScreenContext();
            return KErrNone;
        case EExtensionDebugDeviceGuid:
            aRetPtr=(TAny*)Device();
            return KErrNone;
        case EExtensionDebugElementGuid:
            if (aExtra)
                {
                CElementWrapper* newElement=static_cast<CElementWrapper*>(aExtra);
                aRetPtr=(TAny*)newElement->Element();
                return KErrNone;
                }
            else
                {
                return KErrBadHandle;
                }
        case EExtensionDebugSourceGuid:
            if (aExtra)
                {
                TSurfaceId* surfId=static_cast<TSurfaceId*>(aExtra);
                SourceAndRef* foundSourceAndRef=iSourceMap.Find(*surfId);
                if (foundSourceAndRef)
                    {
                    aRetPtr=(TAny*)foundSourceAndRef->source;
                    return KErrNone;
                    }
                else
                    {
                    return KErrNotFound;
                    }
                }
            else
                {
                return KErrBadHandle;
                }
#endif
        case MCompositionSurfaceUpdate::ETypeId:
             {
             CBaseExtension* basePtr = NULL;
             SymbianStreamHasRegisteredScreenNotifications(iScreenNumber,reinterpret_cast<void**>(&basePtr));
             if (basePtr)
                 {
                 return basePtr->Extension_(MCompositionSurfaceUpdate::KUidCompositionSurfaceUpdate,aRetPtr,aExtra);
                 }
             else
                 {
                 return KErrNotSupported;
                 }
             }
        case MWsDisplayControl::ETypeId:
            if (iContextDisplayControl)
                {
                aRetPtr= static_cast<MWsDisplayControl*>(this);
                return KErrNone;
                }
            else
                {
                return KErrNotSupported;
                }
        case MDisplayControlBase::ETypeId:
            if (iContextDisplayControl)
                {
                aRetPtr= static_cast<MDisplayControlBase*>(this);
                return KErrNone;
                }
            else
                {
                return KErrNotSupported;
                }
        default:
            return KErrNotSupported;
        }
    }
MWsElement* COpenWfcWrapper::CreateElementL()
	{
	CElementWrapper* element = CElementWrapper::NewL(*this,iCleanupElementList);
	return element;
	}

void COpenWfcWrapper::DestroyElement(MWsElement* aElement)
	{		
	if(aElement)
	    {	    
	    CElementWrapper* element=static_cast<CElementWrapper*>(aElement);
	    RemoveElementFromSceneList(element);
	    if (!(element->UpdateFlags()&CElementWrapper::EUpdate_SceneCommited))
	          {
	             delete element;
	          }
	    else
	          {
	          // PHOREPOC delta (2026-10-27): an element still in the composed scene was only unlinked here -- never removed
	          // from the wfc scene, never deleted -- so it stayed on screen for good (the ended DSA element of Dungeon
	          // Hunter 2's intro video covered the GL game, white). Queue it like RemoveElement does, flagged Destroy:
	          // FlushSceneElementChanges removes it from the scene and deletes it.
	          RDebug::Printf("PH: alf: wrapper DestroyElement %x still in the scene: queued for removal", (TUint)element);
	          element->InsertAfter(iRemoveElementList,NULL,CElementWrapper::EUpdate_Destroy);
	          SetCompositionModified();
	          }
	    }
	}

void COpenWfcWrapper::Compose(TRequestStatus* aCompleted)
	{
	{	// safety net (ALF): a UI element with a surface but an EMPTY destination/source (WSERV set the rectangles while the element had no surface and the
		// setters refuse then) is shown full screen -- the alf window's surface is the whole UI
	const TSize scr = ScreenSize();
	for (CElementWrapper* e = iSceneElementList; e; e = e->MyElementAbove())
		{
		TUint32 f = 0;
		e->GetRenderStageFlags(f);
		TRect d, sr;
		if ((f & 3) && !e->ConnectedSurface().IsNull() && e->GetDestinationRectangle(d) == KErrNone && d.IsEmpty() && scr.iWidth > 0)
			{
			e->GetSourceRectangle(sr);
			const TInt e1 = sr.IsEmpty() ? e->SetSourceRectangle(TRect(scr)) : KErrNone;
			const TInt e2 = e->SetDestinationRectangle(TRect(scr));
			RDebug::Printf("PH: alf: wrapper: UI element had an empty destination -> full screen %dx%d (r %d %d)", scr.iWidth, scr.iHeight, e1, e2);
			}
		}
	}
	{
	static TInt phN;
	static TUint32 phSig;	// the scene's elements + surfaces: print again whenever it changes
	TUint32 sig = 0;
	for (CElementWrapper* e = iSceneElementList; e; e = e->MyElementAbove())
		sig = sig * 31 + (TUint32)e + e->ConnectedSurface().iInternal[0] + e->ConnectedSurface().iInternal[3];
	const TBool changed = sig != phSig;
	phSig = sig;
	if (++phN <= 6 || (phN & 63) == 0 || changed)
		{
		TInt n = 0;
		for (CElementWrapper* e = iSceneElementList; e; e = e->MyElementAbove())
			n++;
		RDebug::Printf("PH: alf: wrapper Compose #%d: %d scene elements (bottom to top)", phN, n);
		TInt k = 0;
		for (CElementWrapper* e = iSceneElementList; e && k < 12; e = e->MyElementAbove(), k++)
			{
			TRect d;
			e->GetDestinationRectangle(d);
			const TSurfaceId& sid = e->ConnectedSurface();
			TUint32 fl = 0;
			e->GetRenderStageFlags(fl);
			RDebug::Printf("PH: alf:   element %d (%x flags %x%s): surface %s %08x:%08x:%08x:%08x dest %d,%d-%d,%d", k, (TUint)e, fl,
				e == iUiElement1 ? " ui1" : e == iUiElement2 ? " ui2" : "", sid.IsNull() ? "NULL" : "set",
				sid.iInternal[0], sid.iInternal[1], sid.iInternal[2], sid.iInternal[3], d.iTl.iX, d.iTl.iY, d.iBr.iX, d.iBr.iY);
			}
		}
	}
	STD_ASSERT_DEBUG(CompositionDue(), EPluginPanicCompositionSequenceError);

	OPENWFC_ASSERT_DEBUG(iJobManager, EPanicWfcThreadManagerNotInitialised);
    if (!iAutonomousCompositionInitiated)
        {
        ResumeComposition();
        }
    iJobManager->ComposeRequest(aCompleted);
    
	iCompositionModified = EFalse;
	ResumeCompositorIfPaused();
	}

TSize COpenWfcWrapper::ScreenSize() const
	{
    // clean previous errors
    #ifdef _DEBUG
    (void)wfcGetError(iDevice);
    #endif
    
    TInt height = wfcGetContextAttribi(iDevice, iOnScreenContext, WFC_CONTEXT_TARGET_HEIGHT);
    OPENWFC_ASSERT_DEBUG(TranslateOpenWfcError(wfcGetError(iDevice)) == KErrNone, EPanicWfcBadAttribute);

    TInt width = wfcGetContextAttribi(iDevice, iOnScreenContext, WFC_CONTEXT_TARGET_WIDTH);
    OPENWFC_ASSERT_DEBUG(TranslateOpenWfcError(wfcGetError(iDevice)) == KErrNone, EPanicWfcBadAttribute);
    
    WFCint rotation = wfcGetContextAttribi(iDevice, iOnScreenContext, WFC_CONTEXT_ROTATION);
    OPENWFC_ASSERT_DEBUG(TranslateOpenWfcError(wfcGetError(iDevice)) == KErrNone, EPanicWfcBadAttribute);
    
    return rotation == WFC_ROTATION_0 || rotation == WFC_ROTATION_180 ? TSize(width, height) : TSize(height, width);
	}

TInt COpenWfcWrapper::RegisterSurface (const TSurfaceId& aSurface)
    {
    // clean previous errors
    wfcGetError(iDevice);
    //check surface id not null
    if (aSurface.IsNull())
        {
        return KErrArgument;
        }
    
    if (!IsValidSurface(aSurface))
        {
        return KErrBadHandle;
        } 
    
    //look for surface being registered
    SourceAndRef* sourceAndRef = iSourceMap.Find(aSurface);
    if (sourceAndRef)
        {
        //if registered increase ref count and return
        sourceAndRef->sourceRef++;
        return KErrNone;
        }

    // Surface not currently registered so do so now.
    // Initialise struct. Could be handled via. constructor.
    SourceAndRef newSourceAndRef;
    newSourceAndRef.source = NULL;
    newSourceAndRef.sourceRef = 1;    //Surface reference count
    newSourceAndRef.elementRef = 0;   //Element reference count

    //No need to acquire stream as public NativeStream handle is TSurfaceId
    WFCNativeStreamType stream = reinterpret_cast<WFCNativeStreamType>(&aSurface);

    //create source
    RDebug::Printf("PH: alf: wrapper wfcCreateSourceFromStream ...");
    newSourceAndRef.source = wfcCreateSourceFromStream(iDevice,iOnScreenContext,stream,NULL);
    RDebug::Printf("PH: alf: wrapper wfcCreateSourceFromStream -> %x", (TUint)newSourceAndRef.source);
    //check for valid surface size should be performed by compositor in CreateSource.
    if (newSourceAndRef.source == WFC_INVALID_HANDLE)
        {
        return TranslateOpenWfcError(wfcGetError(iDevice));
        }
    //add to list
    TInt err = iSourceMap.Insert(aSurface,newSourceAndRef);
    if (err != KErrNone)
        {
        wfcDestroySource(iDevice,newSourceAndRef.source);
        return KErrNoMemory;
        }

    return KErrNone;
    }

TInt COpenWfcWrapper::UnregisterSurface (const TSurfaceId& aSurface)
    {
    //check surface id not null
    if (aSurface.IsNull())
        {
        return KErrArgument;
        }

    if (!IsValidSurface(aSurface))
        {
        return KErrArgument;
        }   
    
    SourceAndRef* sourceAndRef = iSourceMap.Find(aSurface);

    if (sourceAndRef && sourceAndRef->sourceRef == 0)
        {
        sourceAndRef = NULL;
        }
    if (sourceAndRef)
        {
        if (sourceAndRef->sourceRef <= 1)
            {
            sourceAndRef->sourceRef = 0;

            if (sourceAndRef->elementRef > 0)
                {
                //if elements are currently in use
                return KErrInUse;
                }
            wfcDestroySource(iDevice,sourceAndRef->source); //removing source reference on the stream

            //destroy entry
            iSourceMap.Remove(aSurface);
            return KErrNone;
            }
        sourceAndRef->sourceRef--;
        return KErrNone;
        }
    return KErrBadHandle;
    }

TBool COpenWfcWrapper::IsValidSurface(const TSurfaceId& aSurface)
    {
    if (aSurface.Type() != TSurfaceId::EScreenSurface)
        {
        RSurfaceManager::TInfoBuf buff;
        TInt err = SurfaceManager().SurfaceInfo(aSurface, buff);
    
        if (err == KErrArgument)
            {
            return EFalse;
            }     
        } 
    return ETrue;
    }

/** 
 * Finds the source for a registered surface ID and increments its "element" reference count
 * @return registered source handle or WFC_INVALID_HANDLE if not registered
 **/
WFCSource COpenWfcWrapper::IncEltRefRegSource(const TSurfaceId& aHashKey)
    {
    SourceAndRef* foundSourceAndRef=iSourceMap.Find(aHashKey);
    if (foundSourceAndRef && foundSourceAndRef->sourceRef)
        {   //still registered
        foundSourceAndRef->elementRef++;
        return foundSourceAndRef->source;
        }
    return WFC_INVALID_HANDLE;
    }


/** 
 * Finds the source for a registered surface ID and decrements the "element" reference count.
 * If all counts are zero then the source is unregistered and the mapping removed   
 * @return positive if not destroyed, 0 if destroyed
 *         or KErrBadHandle if bad surface id or surface has no element reference count
 **/
TInt COpenWfcWrapper::DecEltRefRegSource(const TSurfaceId& aHashKey)
    {
    SourceAndRef* foundSourceAndRef=iSourceMap.Find(aHashKey);
    if (foundSourceAndRef && foundSourceAndRef->elementRef>0)
        {
        TInt retRefs=((--foundSourceAndRef->elementRef)+foundSourceAndRef->sourceRef);
        if (retRefs==0)
            {
            wfcDestroySource(iDevice,foundSourceAndRef->source);
            iSourceMap.Remove(aHashKey);
            }
        return retRefs;
        }
    return KErrBadHandle;
    }

void COpenWfcWrapper::RemoveElement(MWsElement* aElement)
	{
    if (!aElement)
        {
        return;
        }
	
	CElementWrapper* removeElement = static_cast<CElementWrapper*>(aElement);
	RDebug::Printf("PH: alf: wrapper RemoveElement %x", (TUint)removeElement);
	
	//the UI element is a special case since we set and maintain the surface
	TUint32 renderStageFlags;
	removeElement->GetRenderStageFlags(renderStageFlags);
	if (renderStageFlags & MWsElement::EElementIsIndirectlyRenderedUserInterface || 
	    renderStageFlags & MWsElement::EElementIsDirectlyRenderedUserInterface)
		{
		if(removeElement == iUiElement1)
		    iUiElement1 = NULL;
		else if(removeElement == iUiElement2)
		    iUiElement2 = NULL;
		else
			STD_ASSERT_DEBUG(EFalse, EPluginPanicNonMatchingRemovalOfUiElement);
		}

	RemoveElementFromSceneList(removeElement);
	removeElement->InsertAfter(iRemoveElementList,NULL,CElementWrapper::EUpdate_Remove);  //insert into remove element list!!!
	SetCompositionModified();
	}

void COpenWfcWrapper::RemoveElementFromSceneList(CElementWrapper* aElement)
    {
    CElementWrapper** refFromElementBelow=NULL;
    if (iSceneElementList==aElement)
        {
        refFromElementBelow=&iSceneElementList;  //remove from bottom of scene
        }
    else if (iRemoveElementList==aElement)
        {
        refFromElementBelow=&iRemoveElementList;
        }
    else
        {
        refFromElementBelow=aElement->RefsMyElementBelow();   //remove from current mid-list position
        }
    
    aElement->RemoveAfter(refFromElementBelow);
    }

// PHOREPOC delta (2026-10-15, the rotated frame was black): a UI element that covered the whole old UI surface follows
// the new one -- source AND destination rectangle = the new surface's full size. The render stage swaps the UI surface
// on a scene rotation (CDisplayRenderStage::SetSceneRotation: the inverted-aspect 640x360 twin of the 360x640 target),
// but CScreen::CreateUiElementL set the element's rectangles once ("initial guess... updated by PositionUiElements",
// a function the 2010 source does not have): the old 360x640 source rectangle lies outside the 640x360 surface, the
// element is never composed and the screen is the black background (PinePhone boot 499).
static void PhFollowUiSurface(CElementWrapper* aElement, const TSize& aOldSize, const TSize& aNewSize)
	{
	if (!aElement || aNewSize.iWidth <= 0 || aNewSize.iHeight <= 0)
		{
		return;
		}
	TRect src, dst;
	aElement->GetSourceRectangle(src);
	aElement->GetDestinationRectangle(dst);
	if (src == TRect(aOldSize) || src.Size() == TSize(aOldSize.iHeight, aOldSize.iWidth))
		{
		aElement->SetSourceRectangle(TRect(aNewSize));
		}
	if (dst == TRect(aOldSize) || dst.Size() == TSize(aOldSize.iHeight, aOldSize.iWidth))
		{
		aElement->SetDestinationRectangle(TRect(aNewSize));
		}
	}

// OURS (2026-10-27): alf renders a rotated screen itself (its display orientation, EAlfSetScreenRotation) into its portrait
// UI surface, while the compositor rotates the scene for the apps' own surfaces (GL, DSA). The UI element gets the inverse
// source rotation and the whole rotated scene as destination, so the alf layer is not rotated twice (DH2's GL title was
// covered by a portrait-shaped alf layer).
static void PhUiElementForRotation(CElementWrapper* aElement, MWsScene::TSceneRotation aRotation, const TSize& aSurfaceSize)
	{
	if (!aElement)
		{
		return;
		}
	TSize aUiSize = aSurfaceSize;
	if (aUiSize.iWidth <= 0 || aUiSize.iHeight <= 0)
		{	// the alf EGL surface is not always known to the surface manager: its source rectangle is the whole surface
		TRect src;
		aElement->GetSourceRectangle(src);
		aUiSize = src.Size();
		}
	if (aUiSize.iWidth <= 0 || aUiSize.iHeight <= 0)
		{
		RDebug::Printf("PH: alf: UI element %x: no size for the scene rotation", (TUint)aElement);
		return;
		}
	MWsElement::TElementRotation inverse = MWsElement::EElementAntiClockwise0;
	TSize dest = aUiSize;
	switch (aRotation)
		{
		case MWsScene::ESceneAntiClockwise90:	// (the scene's enum maps to WFC_ROTATION_90, the element's
			inverse = MWsElement::EElementAntiClockwise90;	// anticlockwise 90 to WFC_ROTATION_270: they cancel)
			dest = TSize(aUiSize.iHeight, aUiSize.iWidth);
			break;
		case MWsScene::ESceneAntiClockwise180:
			inverse = MWsElement::EElementAntiClockwise180;
			break;
		case MWsScene::ESceneAntiClockwise270:
			inverse = MWsElement::EElementAntiClockwise270;
			dest = TSize(aUiSize.iHeight, aUiSize.iWidth);
			break;
		default:
			break;
		}
	const TInt e1 = aElement->SetSourceRectangle(TRect(aUiSize));
	const TInt e2 = aElement->SetSourceRotation(inverse);
	const TInt e3 = aElement->SetDestinationRectangle(TRect(dest));
	RDebug::Printf("PH: alf: UI element %x for scene rotation %d: source rotation %d dest %dx%d (r %d %d %d)", (TUint)aElement,
		(TInt)aRotation, (TInt)inverse, dest.iWidth, dest.iHeight, e1, e2, e3);
	}

static TSize PhSurfaceSize(RSurfaceManager& aSurfaceManager, const TSurfaceId& aSurface)
	{
	RSurfaceManager::TInfoBuf info;
	if (aSurface.IsNull() || aSurfaceManager.SurfaceInfo(aSurface, info) != KErrNone)
		{
		return TSize(0, 0);
		}
	return info().iSize;
	}

TInt COpenWfcWrapper::SetUiSurface(const TSurfaceId& aNewUiSurface)
	{
	RDebug::Printf("PH: alf: wrapper SetUiSurface");
	const TSize oldSize = PhSurfaceSize(SurfaceManager(), iUiSurface);
	TInt err = RegisterSurface(aNewUiSurface);
	if (!err)
		{
		const TSize newSize = PhSurfaceSize(SurfaceManager(), aNewUiSurface);
		if(iUiElement1)
			{
			err = iUiElement1->ConnectSurface(aNewUiSurface);
			STD_ASSERT_DEBUG(!err, EPluginPanicNewUiSurfaceUnsettable);
			if (!err && oldSize != newSize)
				{
				PhFollowUiSurface(iUiElement1, oldSize, newSize);
				}
			}
		if(iUiElement2)
			{
			err = iUiElement2->ConnectSurface(aNewUiSurface);
			STD_ASSERT_DEBUG(!err, EPluginPanicNewUiSurfaceUnsettable);
			if (!err && oldSize != newSize)
				{
				PhFollowUiSurface(iUiElement2, oldSize, newSize);
				}
			}
		}

	if(!err)
		{
		if(!iUiSurface.IsNull())
			{
			// This should not fail as the surface will no longer be used by any element
			err = UnregisterSurface(iUiSurface);

      if (err == KErrInUse)	// This is not needed once openwf implementation is corrected:
									          // It should not return KErrInUse if it is only the backend holding onto it.
			    {
			    err = KErrNone;
			    }

			STD_ASSERT_DEBUG(!err, EPluginPanicPreviousUiSurfaceUnregisterable);
			}

		iUiSurface = aNewUiSurface;
		}
	return err;
	}

TInt COpenWfcWrapper::AddElement(MWsElement* aElement, MWsElement* aAbove)
    {
    TInt err = KErrNone;
    CElementWrapper* addedElement = static_cast<CElementWrapper*>(aElement);
    CElementWrapper* insertAboveThisElement = static_cast<CElementWrapper*>(aAbove);
    {
    TUint32 phf = 0;
    if (addedElement) addedElement->GetRenderStageFlags(phf);
    RDebug::Printf("PH: alf: wrapper AddElement %x above %x flags %x surface %s", (TUint)aElement, (TUint)aAbove, phf,
        (addedElement && !addedElement->ConnectedSurface().IsNull()) ? "set" : "NULL");
    }

    if (aElement == aAbove)
        {
        return KErrArgument;
        }
    
    if (addedElement == NULL || &addedElement->Owner()!=this || 
        (insertAboveThisElement && &insertAboveThisElement->Owner()!=this) || 
        (addedElement->UpdateFlags() & CElementWrapper::EUpdate_Destroy))
        {
        return KErrArgument;
        }
    
    // OURS (2026-10-27): under Alfred WSERV's own UI element is withheld from the scene (the alf window's element is composed
    // instead), so a surface window (a DSA game) placed "above" it named an element that is not in the scene and was refused:
    // the game layer was never composed (black screen). Such an element goes to the bottom, under the alf UI layer, which is
    // transparent where the window is.
    if (insertAboveThisElement && !insertAboveThisElement->RefsMyElementBelow() && iSceneElementList != insertAboveThisElement &&
        !(insertAboveThisElement->UpdateFlags() & (CElementWrapper::EUpdate_Remove | CElementWrapper::EUpdate_Destroy)))
        {
        RDebug::Printf("PH: alf: wrapper AddElement %x: 'above' %x is not in the scene -> bottom", (TUint)aElement, (TUint)aAbove);
        insertAboveThisElement = NULL;
        }
    if (insertAboveThisElement &&
        ((!insertAboveThisElement->RefsMyElementBelow() && iSceneElementList != insertAboveThisElement) ||
        (insertAboveThisElement->UpdateFlags() & (CElementWrapper::EUpdate_Remove | CElementWrapper::EUpdate_Destroy))))
        {
        return KErrArgument;
        }
    
    //the UI element is a special case since we set and maintain the surface
    TUint32 renderStageFlags;
    addedElement->GetRenderStageFlags(renderStageFlags);
    // OURS (2026-10-27): a DIRECTLY rendered UI element that brings its own surface keeps it -- WSERV flags a portrait DSA
    // window's element so (its surface is the screen's DSA buffer, the game's pixels); attaching the alf UI surface to it
    // showed the alf layer twice and the game never (SkyForce black under Alfred)
    if (renderStageFlags & MWsElement::EElementIsIndirectlyRenderedUserInterface ||
        (renderStageFlags & MWsElement::EElementIsDirectlyRenderedUserInterface && addedElement->ConnectedSurface().IsNull()))
        {
        err = AttachSurfaceToUiElement(addedElement);
        }
    
    if(!err)
        {
        //figure out which element to add the element above           
        const TSurfaceId& newSurf = addedElement->ConnectedSurface();
        if (!addedElement->ConnectedSurface().IsNull())
            {
            //is surface registered
            WFCSource newSource = IncEltRefRegSource(newSurf);
            if (newSource == WFC_INVALID_HANDLE)
                { //suggests the surface was not registered
                return KErrArgument;
                }
            DecEltRefRegSource(newSurf);    //only need to decrement if it found a source
            }
        
        RemoveElementFromSceneList(addedElement);
       
        // figure out which element to add it below
        CElementWrapper** insertBelowThisElement = NULL;
        if (insertAboveThisElement)
            {
            insertBelowThisElement=&insertAboveThisElement->MyElementAbove();
            }
        else
            {
            insertBelowThisElement=&iSceneElementList;
            }
        
        addedElement->InsertAfter(*insertBelowThisElement,insertAboveThisElement,CElementWrapper::EUpdate_Insert);
        
        SetCompositionModified();
        }
    
    return err;
    }

void COpenWfcWrapper::Compose(const TSurfaceId& aOffScreenTarget, TRequestStatus* aCompleted)
	{
    if (aOffScreenTarget.IsNull())
        {
        if (aCompleted)
            {
            User::RequestComplete(aCompleted, KErrNone);
            return;
            }
        }
    ComposeInternal(aOffScreenTarget,TPoint(0,0),iSceneElementList,NULL,aCompleted);
	}

void COpenWfcWrapper::Compose(const TSurfaceId& aOffScreenTarget, const TPoint& aOrigin,
	         MWsElement* aStart, MWsElement* aEnd, TRequestStatus* aCompleted)
	{
    if (aOffScreenTarget.IsNull())
        {
        if (aCompleted)
            {
            User::RequestComplete(aCompleted, KErrNone);
            return;
            }
        }
    
    CElementWrapper* startElement=static_cast<CElementWrapper*>(aStart);
    CElementWrapper* endElement=static_cast<CElementWrapper*>(aEnd);
    
    if (startElement)
        {
        if (    &startElement->Owner()!=this
            ||  (startElement!=iSceneElementList && !startElement->ElementBelow())
            ||  (startElement->UpdateFlags() & (startElement->EUpdate_Remove|startElement->EUpdate_Destroy))
            )
            {
            if (aCompleted)
                {
                RThread().RequestComplete(aCompleted, KErrArgument);
                }
            return;
            }
        if (endElement)
            {   //If endElement is NULL then draw to end else draw including the specified endElement
            TBool fail=EFalse;
            if (    &endElement->Owner()!=this
                ||  (endElement->UpdateFlags() & (endElement->EUpdate_Remove|endElement->EUpdate_Destroy))
                )
                {
                fail=ETrue;
                }
            else
                {
                for (CElementWrapper* sceneElement=startElement;sceneElement!=endElement;sceneElement=sceneElement->MyElementAbove())
                    {
                    if (sceneElement==NULL)
                        {
                        fail=ETrue;
                        break;
                        }
                    }
                }
            if (fail)
                {
                if (aCompleted)
                    {
                    RThread().RequestComplete(aCompleted, KErrArgument);
                    }
                return;
                }
            aEnd=aEnd->ElementAbove();   //now scene does NOT include endElement. May now be NULL!
            }
        }
    else
        {
        aEnd=NULL;
        }
    ComposeInternal(aOffScreenTarget,aOrigin,aStart,aEnd,aCompleted);
	}

/** Internal compose does not check some errors. 
 *  They should have already been checked by the public API caller.
 *  NOTE element aEnd is NOT composed by this internal method!
 *  Any elements pending removal will not be displayed (including aStart and aEnd.
 * 
 * @param   aOffScreenTarget    A valid surface for offscreen composition.
 * @param   aOrigin             The origin sets where the top-left corner of the
 *                              surface is within the element coordinate system.
 * @param   aStart              The starting element of the elements to be composed.
 * @param   aEnd                The element above the last element to be composed (or NULL).
 * @param   aCompleted          NULL or request status that will be set to KErrNone when
 *                              the composition finishes.
 **/
void COpenWfcWrapper::ComposeInternal (const TSurfaceId&   aOffScreenTarget,
                        const TPoint&       aOrigin,
                        MWsElement*       aStart,
                        MWsElement*       aEnd,
                        TRequestStatus*     aCompleted)
    {
    // Initial approach reuses the device but it may be more safer to create, and destroy, its own device
    CElementWrapper* startElement=static_cast<CElementWrapper*>(aStart);
    CElementWrapper* endElement=static_cast<CElementWrapper*>(aEnd);
    
    TInt err = KErrNone;
#ifdef _DEBUG
    // clean previous errors
    wfcGetError(iDevice);
#endif    
    
    if (aCompleted)
        {
        *aCompleted = KRequestPending;
        }
    
    OffScreenComposeGuard composeGuard(aCompleted);
    
    WFCNativeStreamType stream=reinterpret_cast<WFCNativeStreamType>(&aOffScreenTarget);
    
    RSurfaceManager::TInfoBuf buff;
    err=SurfaceManager().SurfaceInfo(aOffScreenTarget,buff);
    OPENWFC_ASSERT_DEBUG(err==KErrNone,TOpenWfcPanic(__LINE__));
    if (err!=KErrNone || buff().iSize.iWidth ==0 || buff().iSize.iHeight == 0)
        {
        composeGuard.LogRequestStatusError(KErrArgument);
        composeGuard.Close();
        return;
        }
    
    // Create the native stream target
    TSurfaceId targetSurfaceId = TSurfaceId::CreateNullId();
    WFCNativeStreamType targetStream = WFC_INVALID_HANDLE;
    OPENWFC_ASSERT_DEBUG(wfcGetError(iDevice)==WFC_ERROR_NONE,(TOpenWfcPanic)__LINE__);

    targetSurfaceId = aOffScreenTarget;
    targetStream = stream;
     
    EGLint attrib_list[1] = {EGL_NONE };
    
    EGLSyncKHR sync = eglCreateSyncKHR(iEGLDisplay,
                                                EGL_SYNC_REUSABLE_KHR,
                                                attrib_list);
    
    if (sync == EGL_NO_SYNC_KHR)
        {
        composeGuard.LogRequestStatusError(KErrNoMemory);
        composeGuard.Close();
        return;
        }
    composeGuard.SetSync(sync, iEGLDisplay);
    
    // Now, that we have the target surface stream we can create the temporary context
    WFCContext offScreenContext = wfcCreateOffScreenContext(iDevice, targetStream, NULL);

    if (offScreenContext == WFC_INVALID_HANDLE)
        {
        composeGuard.LogRequestStatusError(TranslateOpenWfcError(wfcGetError(iDevice)));
        composeGuard.Close();
        return;
        }

    composeGuard.SetDeviceAndContext(iDevice, offScreenContext);

    TInt contextBackGroundColour = wfcGetContextAttribi(iDevice, iOnScreenContext, WFC_CONTEXT_BG_COLOR);
    wfcSetContextAttribi(iDevice, offScreenContext, WFC_CONTEXT_BG_COLOR, contextBackGroundColour);
    OPENWFC_ASSERT_DEBUG(TranslateOpenWfcError(wfcGetError(iDevice)) == KErrNone, EPanicWfcBadAttribute);
    
    TPoint elementOffset(-aOrigin.iX,-aOrigin.iY);
    // let's replicate the scene to the offscreen context
    WFCElement previousTargetElement = WFC_INVALID_HANDLE;
    for (CElementWrapper* elementScene = startElement; elementScene !=endElement; elementScene = elementScene->MyElementAbove())
        {
        if (!(elementScene->UpdateFlags()&(elementScene->EUpdate_Destroy|elementScene->EUpdate_Remove)))
            {
            // we do our best to replicate the elements
            TInt err=elementScene->ReplicateElements(offScreenContext, previousTargetElement,elementOffset);
            if (err)
                {
                composeGuard.LogRequestStatusError(err);
                composeGuard.Close();
                return;
                }
            }
        }

    // let's compose
    wfcCommit(iDevice, offScreenContext, WFC_TRUE);
    wfcCompose(iDevice, offScreenContext, WFC_TRUE);
    wfcFence(iDevice, offScreenContext, iEGLDisplay, sync);
    EGLTimeKHR timeout = (EGLTimeKHR) EGL_FOREVER_KHR;
    OPENWFC_ASSERT_DEBUG(TranslateOpenWfcError(wfcGetError(iDevice)) == KErrNone, EPanicWfcBadAttribute);
    eglClientWaitSyncKHR(iEGLDisplay, sync, 0, timeout);
    
    composeGuard.Close();
    }

void COpenWfcWrapper::PauseComposition (void)
    {
    
    OPENWFC_ASSERT_DEBUG(iJobManager, EPanicWfcThreadManagerNotInitialised);
    iAutonomousCompositionInitiated = ETrue;
    iJobManager->CompositionPauseRequest();
    }

void COpenWfcWrapper::ResumeComposition (void)
    {
    OPENWFC_ASSERT_DEBUG(iJobManager, EPanicWfcThreadManagerNotInitialised);
    iAutonomousCompositionInitiated = ETrue;
    iJobManager->CompositionResumeRequest();
    }

TBitFlags32 COpenWfcWrapper::SupportedScreenRotations() const
    {
    TBitFlags32 result;
    // we DO support, by default, all these rotations
    result.Set(MWsScene::ESceneAntiClockwise0);
    result.Set(MWsScene::ESceneAntiClockwise90);
    result.Set(MWsScene::ESceneAntiClockwise180);
    result.Set(MWsScene::ESceneAntiClockwise270);
    return result;
    }

void COpenWfcWrapper::SetScreenRotation(MWsScene::TSceneRotation aRotation)
	{
	RDebug::Printf("PH: alf: wrapper SetScreenRotation %d", (TInt)aRotation);
#ifdef _DEBUG
    // clean previous errors
    (void)wfcGetError(iDevice);
#endif

    if (aRotation != iRotation)
        {
        WFCRotation wfcRotation = WFC_ROTATION_0;
        switch (aRotation)
            {
            case MWsScene::ESceneAntiClockwise90:
                wfcRotation = WFC_ROTATION_90;
                break;
            case MWsScene::ESceneAntiClockwise180:
                wfcRotation = WFC_ROTATION_180;
                break;
            case MWsScene::ESceneAntiClockwise270:
                wfcRotation = WFC_ROTATION_270;
                break;
            default:
                wfcRotation = WFC_ROTATION_0;
                break;
            }
        iRotation = aRotation;
        wfcSetContextAttribi(iDevice, iOnScreenContext, WFC_CONTEXT_ROTATION, wfcRotation);
        OPENWFC_ASSERT_DEBUG(TranslateOpenWfcError(wfcGetError(iDevice)) == KErrNone, EPanicWfcBadAttribute);
        const TSize uiSize = PhSurfaceSize(SurfaceManager(), iUiSurface);
        PhUiElementForRotation(iUiElement1, aRotation, uiSize);
        PhUiElementForRotation(iUiElement2, aRotation, uiSize);
        }
	
	SetCompositionModified();
	}

TInt COpenWfcWrapper::SetConfiguration(const TDisplayConfiguration& aConfig)
    {
    return iContextDisplayControl->SetConfiguration (aConfig);
    }

MWsScene::TSceneRotation COpenWfcWrapper::ScreenRotation() const
	{
#ifdef _DEBUG
    // clean previous errors
    (void)wfcGetError(iDevice);
#endif
    WFCint result = wfcGetContextAttribi(iDevice, iOnScreenContext, WFC_CONTEXT_ROTATION);
    OPENWFC_ASSERT_DEBUG(TranslateOpenWfcError(wfcGetError(iDevice)) == KErrNone, EPanicWfcBadAttribute);
    MWsScene::TSceneRotation rotation = MWsScene::ESceneAntiClockwise0;
    switch (result)
        {
        case WFC_ROTATION_90:
            rotation = MWsScene::ESceneAntiClockwise90;
            break;
        case WFC_ROTATION_180:
            rotation = MWsScene::ESceneAntiClockwise180;
            break;
        case WFC_ROTATION_270:
            rotation = MWsScene::ESceneAntiClockwise270;
            break;
        case WFC_ROTATION_0:
        default:
            OPENWFC_ASSERT_DEBUG(result==WFC_ROTATION_0,(TOpenWfcPanic)__LINE__);
            rotation = MWsScene::ESceneAntiClockwise0;
            break;
        }
    
	return rotation;
	}

TInt COpenWfcWrapper::NumberOfResolutions()const
    {
    if (iContextDisplayControl)
        {
        return iContextDisplayControl->NumberOfResolutions();
        }
    return KErrNotSupported;
    }

TInt COpenWfcWrapper::GetResolutions(RArray<TResolution>& aResolutions)const
    {
    if (iContextDisplayControl)
        {
        return iContextDisplayControl->GetResolutions(aResolutions);
        }
    return KErrNotSupported;
    }

void COpenWfcWrapper::GetConfiguration(TDisplayConfiguration& aConfig)const
    {
    if (iContextDisplayControl)
        {
        iContextDisplayControl->GetConfiguration (aConfig);
        }
    }

TInt COpenWfcWrapper::PreferredDisplayVersion() const
    {
    if (iContextDisplayControl)
        {
        return iContextDisplayControl->PreferredDisplayVersion();
        }
    return KErrNotSupported;
    }

void COpenWfcWrapper::NotifyOnDisplayChange(TRequestStatus& aStatus)
    {
    // Stub for future implementation. 
    TRequestStatus *status = &aStatus;
    RThread().RequestComplete(status, KErrNotSupported);
    }

void COpenWfcWrapper::NotifyOnDisplayChangeCancel()
    {
    // Stub for future implementation. 
    }

void COpenWfcWrapper::NotifyOnConfigChange(TRequestStatus& aStatus)
    {
    // Stub for future implementation. 
    TRequestStatus *status = &aStatus;
    RThread().RequestComplete(status, KErrNotSupported);
    }

void COpenWfcWrapper::NotifyOnConfigChangeCancel()
    {
    // Stub for future implementation. 
    }


void COpenWfcWrapper::FlushSceneElementChanges()
    {
    while (CElementWrapper* curr = iRemoveElementList)
        {
        curr->FlushSceneChanges();
        RemoveElementFromSceneList(curr);
        if (curr->UpdateFlags() & CElementWrapper::EUpdate_Destroy)
            {
            delete curr;
            }
        }
    iRemoveElementList=NULL;
    if (CElementWrapper* curr=iSceneElementList)
        {
        for (CElementWrapper*next=curr->MyElementAbove(); curr;   curr=next,next=curr?curr->MyElementAbove():NULL)
            {
            curr->FlushSceneChanges();
            }
        }
    }

TInt COpenWfcWrapper::TranslateOpenWfcError(WFCErrorCode error)
    {
    switch (error)
        {
        case WFC_ERROR_NONE:
            return KErrNone;
        case WFC_ERROR_OUT_OF_MEMORY:
            return KErrNoMemory;
        case WFC_ERROR_ILLEGAL_ARGUMENT:
            return KErrArgument;
        case WFC_ERROR_BAD_ATTRIBUTE:
            return KErrArgument;
        case WFC_ERROR_BAD_DEVICE:
            return KErrArgument;
        case WFC_ERROR_UNSUPPORTED:
            return KErrNotSupported;
        case WFC_ERROR_IN_USE:
            return KErrInUse;
        case WFC_ERROR_BUSY:
            return KErrServerBusy;
        case WFC_ERROR_BAD_HANDLE:
            return KErrBadHandle;
        case WFC_ERROR_INCONSISTENCY:
            return KErrGeneral;
        case WFC_ERROR_FORCE_32BIT:
            // intentional fall through
        default:
            return KErrUnknown;
        }
    }

void COpenWfcWrapper::DestroyAllContextElements(WFCDevice dev, WFCContext ctx)
    {
    // we try our best to destroy all elements of the specified context
    if (dev != WFC_INVALID_HANDLE && ctx != WFC_INVALID_HANDLE)
        {
        WFCElement element = wfcGetContextAttribi(dev, ctx, WFC_CONTEXT_LOWEST_ELEMENT);
        if (element != WFC_INVALID_HANDLE)
            {
            wfcDestroyElement(dev, element);
            }
        }
    }

void COpenWfcWrapper::PauseCompositorIfNeeded()
	{
	if (!iPaused && iAllowCompositorPause && iWithinBeginEnd)
		{
		PauseComposition();
		iPaused = ETrue;
		}
	}

/**
Sets the system state for modified scene composition.
	- Pause OpenWF composition if the standard render stage is within a begin/end block.
	- If not within a pause/end block then set a flag to indicate that the scene composition 
		has been modified. On entering the begin/end block composition will be paused appropriatly.
*/
void COpenWfcWrapper::SetCompositionModified()
	{
	PauseCompositorIfNeeded();
	iCompositionModified = ETrue;
	}

void COpenWfcWrapper::ResumeCompositorIfPaused()
	{
	if (iPaused)
		ResumeComposition();
	iPaused = EFalse;
	}

void COpenWfcWrapper::Begin(const TRegion* aRegion)
	{
	iWithinBeginEnd = ETrue;
	iAllowCompositorPause = (aRegion && !aRegion->IsEmpty());
	
	if (iCompositionModified)
		PauseCompositorIfNeeded();
	}

TBool COpenWfcWrapper::CompositionDue()
	{
	return (iWithinBeginEnd && iCompositionModified);
	}

void COpenWfcWrapper::End()
	{
	iWithinBeginEnd = EFalse;
	}

TInt COpenWfcWrapper::FlushedSetConfiguration(const TDisplayConfiguration& /*aConfig*/)
	{
	OPENWFC_ASSERT_DEBUG(0, EPanicMethodNotImplemented);
	TInt retval=KErrNone;
	if (retval<=KErrNone)
		{
		SetCompositionModified();
		}
	return retval;
	}

CDisplayPolicy* COpenWfcWrapper::DisplayPolicy()
	{
	return iDisplayPolicy;
	}

TInt COpenWfcWrapper::AttachSurfaceToUiElement(CElementWrapper* aNewUiElement)
	{
	STD_ASSERT_DEBUG(!iUiSurface.IsNull(), EPluginPanicUiSurfaceIsNull);
	if(iUiElement1 == NULL || iUiElement1 == aNewUiElement)
	    iUiElement1 = aNewUiElement;
	else if(iUiElement2 == NULL || iUiElement2 == aNewUiElement)
	    iUiElement2 = aNewUiElement;
	else
		STD_ASSERT_DEBUG(EFalse, EPluginPanicDuplicateUiElement);

	const TInt err = aNewUiElement->ConnectSurface(iUiSurface);
	if (!err && iRotation != MWsScene::ESceneAntiClockwise0)
		{
		PhUiElementForRotation(aNewUiElement, iRotation, PhSurfaceSize(SurfaceManager(), iUiSurface));
		}
	return err;
	}
