// Copyright The Phorepoc Authors
// SPDX-License-Identifier: BSD-2-Clause

// phecomp.cpp -- OURS: Phorepoc's window compositor as a render stage of the NGA window server
// (docs: core os/src/phecomp/README.md). v0. Default (no PHECOMPTILE in wsini): a PASS-THROUGH stage that sits first in the
// render-stage chain (wsini "RENDERSTAGES phecomp flickerbuffer display") and only watches:
//   - the window tree (MWsWindowTreeObserver): window groups and their top-level windows as they are created, placed,
//     restacked, shown and released;
//   - the draw brackets (MWsDrawAnnotationObserver): which window (and so which window group) every redraw belongs to.
// Everything else goes to the next stage unchanged, so the screen is what the standard chain makes of it. The log is
// bounded: one line per window-group event, the per-group redraw counts once a second while something draws.
// The window server finds the observers by asking the FIRST stage of the chain (CScreen: ObjectInterface<...>), which is
// why this stage must be first.
#include <e32base.h>
#include <e32debug.h>
#include <ecom/implementationproxy.h>
#include <graphics/wsplugin.h>
#include <graphics/wsrenderstage.h>
#include <graphics/wsrenderstagefactory.h>
#include <graphics/wsgraphicdrawerinterface.h>
#include <graphics/wsgraphicscontext.h>
#include <graphics/wsdrawablesourceprovider.h>
#include <graphics/wsscreendevice.h>
#include "rendertarget.h"			// the 2010 std plug-in's off-screen target (an RSgImage + a DirectGDI context)

namespace {

const TInt KMaxGroups = 32;
const TInt KMaxTiles = 2;

// Compose mode (wsini PHECOMPTILE <uid> <uid>): every window group draws into its own off-screen target (the redraw
// brackets route the window server's drawing there; CHANGETRACKING makes it draw every window whole, covered or not), and
// End() composes the targets back to front. The groups named in PHECOMPTILE are the tiles: when two of them run, they are
// drawn side by side, scaled; otherwise every group is drawn where the window server put it.
class TGcProxy : public MWsGraphicsContext
	{
public:
	MWsGraphicsContext* iTarget;
	inline MWsGraphicsContext* T() const { return iTarget; }
	TAny* ResolveObjectInterface(TUint aTypeId)
		{
		if (aTypeId == MWsGraphicsContext::EWsObjectInterfaceId)
			return static_cast<MWsGraphicsContext*>(this);
		return iTarget->ResolveObjectInterface(aTypeId);
		}
#include "phecompfwd.inc"
	};

// what the stage knows of one window group
struct TGroup
	{
	const MWsWindowTreeNode* iNode;
	TInt iId;						// the window group identifier (RWindowGroup::Identifier)
	TBuf8<40> iName;				// the start of its name (the app's caption follows the UID)
	TInt iRedraws;					// window redraw brackets since the last summary
	TRect iBounds;					// the union of its top-level windows' extents
	TUint32 iUid;					// the app UID from the name ("<nn>.<uid hex>.<caption>"), 0 if none
	CRenderTarget* iTarget;			// compose mode: its off-screen target (made at its first redraw)
	TAny* iSource;					// ... as a drawable source of the next stage
	};

NONSHARABLE_CLASS(CPheCompStage) : public CWsRenderStage, public MWsWindowTreeObserver, public MWsDrawAnnotationObserver
	{
public:
	static CPheCompStage* NewL(CWsRenderStage* aNext, MWsIniFile* aIni);
	~CPheCompStage();
	// CWsRenderStage
	void Begin(const TRegion* aRegion);
	void End(TRequestStatus* aFinished);
	TAny* ResolveObjectInterface(TUint aTypeId);
	// MWsWindowTreeObserver
	void NodeCreated(const MWsWindowTreeNode& aNode, const MWsWindowTreeNode* aParent);
	void NodeReleased(const MWsWindowTreeNode& aNode);
	void NodeActivated(const MWsWindowTreeNode& aNode);
	void NodeExtentChanged(const MWsWindowTreeNode& aNode, const TRect& aRect);
	void SiblingOrderChanged(const MWsWindowTreeNode& aNode, TInt aNewPos);
	void FlagChanged(const MWsWindowTreeNode& aNode, TFlags aFlag, TBool aNewValue);
	void AttributeChanged(const MWsWindowTreeNode&, TAttributes) {}
	void FadeCountChanged(const MWsWindowTreeNode&, TInt) {}
	void TransparentRegionChanged(const MWsWindowTreeNode&, const TRegion&, const TRegion*) {}
	void ElementAdded(const MWsWindowTreeNode& aNode, const MWsElement& aElement);
	void MovedToWindowGroup(const MWsWindowTreeNode&, const MWsWindowTreeNode&) {}
	void WindowGroupChained(const MWsWindowTreeNode& aParent, const MWsWindowTreeNode& aChild);
	void WindowGroupChainBrokenAfter(const MWsWindowTreeNode&) {}
	void FadeAllChildren(const MWsWindowTreeNode&, TBool) {}
	// MWsDrawAnnotationObserver
	void WindowRedrawStart(const MWsWindowTreeNode& aNode, const TRegion& aRegion);
	void WindowRedrawEnd(const MWsWindowTreeNode& aNode);
	void WindowAnimRedrawStart(const MWsWindowTreeNode&, const TRegion&) {}
	void WindowAnimRedrawEnd(const MWsWindowTreeNode&) {}
	void SpriteRedrawStart(const MWsWindowTreeNode&, const TRegion&) {}
	void SpriteRedrawEnd(const MWsWindowTreeNode&) {}
	void SpriteFlash(const MWsWindowTreeNode&, TBool) {}
	void SegmentRedrawStart(const TRegion&) {}
	void SegmentRedrawEnd() {}
private:
	CPheCompStage() {}
	void ConstructComposeL(MWsIniFile* aIni);
	TGroup* Group(const MWsWindowTreeNode* aNode);
	static const MWsWindowTreeNode* GroupOf(const MWsWindowTreeNode& aNode);
	void Summary();
	TInt MakeTarget(CRenderTarget*& aTarget, TAny*& aSource);
	void DropTarget(CRenderTarget*& aTarget, TAny*& aSource);
	TInt TileOf(const TGroup& aGroup) const;
	void Compose(TRequestStatus* aFinished);
	void SetUid(TGroup& aGroup);
private:
	// compose mode
	TBool iCompose;
	TGcProxy iProxy;
	MWsGraphicsContext* iNextGc;
	MWsDrawableSourceProvider* iSources;
	TSize iSize;							// the targets: square, the screen's longer side (either orientation fits)
	MWsScreenDevice* iDevice;
	TInt iScreenNumber;
	CRenderTarget* iRest;					// drawing outside any window group bracket (root window, sprites)
	TAny* iRestSource;
	TUint32 iTileUid[KMaxTiles];
	TInt iTileUids;
	const MWsWindowTreeNode* iOrder[KMaxGroups];	// the window groups front to back (SiblingOrderChanged)
	TInt iOrderCount;
	TUint32 iComposed;
private:
	TGroup iGroups[KMaxGroups];
	TInt iCount;
	const MWsWindowTreeNode* iDrawing;		// the window inside a redraw bracket (NULL outside)
	TUint32 iFrames, iFramesLogged;
	TUint32 iLastSummary;					// User::NTickCount of the last summary
	};

CPheCompStage* CPheCompStage::NewL(CWsRenderStage* aNext, MWsIniFile* aIni)
	{
	CPheCompStage* self = new(ELeave) CPheCompStage;
	CleanupStack::PushL(self);
	self->BaseConstructL();
	self->SetNext(aNext);
	self->ConstructComposeL(aIni);
	CleanupStack::Pop(self);
	RDebug::Printf("PH: [phecomp] stage up (%s)", self->iCompose ? "compose" : "trace, pass-through");
	return self;
	}

// PHECOMPTILE <uid hex> [<uid hex>]: compose mode, these apps are the tiles
void CPheCompStage::ConstructComposeL(MWsIniFile* aIni)
	{
	_LIT(KTile, "PHECOMPTILE");
	TPtrC tiles;
	if (!aIni || !aIni->FindVar(KTile, tiles))
		return;
	TLex lex(tiles);
	while (iTileUids < KMaxTiles && !lex.Eos())
		{
		lex.SkipSpace();
		TUint32 uid;
		if (lex.Val(uid, EHex) != KErrNone)
			break;
		iTileUid[iTileUids++] = uid;
		}
	iNextGc = Next()->ObjectInterface<MWsGraphicsContext>();
	iSources = iNextGc ? iNextGc->ObjectInterface<MWsDrawableSourceProvider>() : NULL;
	MWsScreenDevice* dev = Next()->ObjectInterface<MWsScreenDevice>();
	if (!iSources || !dev)
		{
		RDebug::Printf("PH: [phecomp] next stage has no GC / drawable sources / screen device: trace only");
		return;
		}
	iDevice = dev;
	const TSize s = dev->SizeInPixels();
	iSize = TSize(Max(s.iWidth, s.iHeight), Max(s.iWidth, s.iHeight));
	iScreenNumber = dev->ScreenNumber();
	User::LeaveIfError(MakeTarget(iRest, iRestSource));
	iProxy.iTarget = iRest->DirectGdiGc();
	iCompose = ETrue;
	RDebug::Printf("PH: [phecomp] compose %dx%d, tiles %08x %08x", iSize.iWidth, iSize.iHeight, iTileUid[0], iTileUid[1]);
	}

// an off-screen target the size of the screen, with alpha (a group that draws only part of the screen leaves the rest
// transparent), cleared, and known to the next stage as a drawable source
TInt CPheCompStage::MakeTarget(CRenderTarget*& aTarget, TAny*& aSource)
	{
	aTarget = NULL;
	aSource = NULL;
	TRAPD(err, aTarget = CRenderTarget::NewL(NULL, ESgUsageDirectGdiTarget | ESgUsageWindowGcSource, ESgCpuAccessReadWrite,
											 EColor16MAP, iSize, iScreenNumber));
	if (err == KErrNone)
		err = iSources->CreateDrawableSource(aTarget->ImageId(CRenderTarget::ENormalAspectRatio), aSource);
	if (err != KErrNone)
		{
		RDebug::Printf("PH: [phecomp] off-screen target: %d", err);
		DropTarget(aTarget, aSource);
		return err;
		}
	MWsGraphicsContext* gc = aTarget->DirectGdiGc();
	gc->SetDrawMode(MWsGraphicsContext::EDrawModeWriteAlpha);
	gc->SetBrushColor(TRgb(0, 0, 0, 0));
	gc->Clear();
	gc->SetDrawMode(MWsGraphicsContext::EDrawModePEN);
	return KErrNone;
	}

void CPheCompStage::DropTarget(CRenderTarget*& aTarget, TAny*& aSource)
	{
	if (aSource && iSources)
		iSources->CloseDrawableSource(aSource);
	aSource = NULL;
	delete aTarget;
	aTarget = NULL;
	}

CPheCompStage::~CPheCompStage()
	{
	for (TInt i = 0; i < iCount; ++i)
		DropTarget(iGroups[i].iTarget, iGroups[i].iSource);
	DropTarget(iRest, iRestSource);
	}

TAny* CPheCompStage::ResolveObjectInterface(TUint aTypeId)
	{
	switch (aTypeId)
		{
	case MWsWindowTreeObserver::EWsObjectInterfaceId:
		return static_cast<MWsWindowTreeObserver*>(this);
	case MWsDrawAnnotationObserver::EWsObjectInterfaceId:
		return static_cast<MWsDrawAnnotationObserver*>(this);
	case MWsGraphicsContext::EWsObjectInterfaceId:
		if (iCompose)
			return static_cast<MWsGraphicsContext*>(&iProxy);
		break;
	case MWsDrawableSourceProvider::EWsObjectInterfaceId:
	case MWsTextCursor::EWsObjectInterfaceId:
	case MWsFader::EWsObjectInterfaceId:
		if (iCompose)
			return iProxy.ResolveObjectInterface(aTypeId);	// the target being drawn now
		break;
		}
	return CWsRenderStage::ResolveObjectInterface(aTypeId);	// the next stage (the GC, the scene, the display ...)
	}

void CPheCompStage::Begin(const TRegion* aRegion)
	{
	if (!iCompose)
		Next()->Begin(aRegion);
	// compose mode: the window server draws into the targets now; End() composes the whole screen
	}

TInt CPheCompStage::TileOf(const TGroup& aGroup) const
	{
	for (TInt i = 0; i < iTileUids; ++i)
		if (aGroup.iUid && aGroup.iUid == iTileUid[i])
			return i;
	return KErrNotFound;
	}

// back to front: the rest (root window, sprites), then every group with a target where the window server put it --
// except the tiles, when both run: side by side, scaled, aspect kept
void CPheCompStage::Compose(TRequestStatus* aFinished)
	{
	TGroup* tile[KMaxTiles] = {NULL, NULL};
	TInt tiles = 0;
	for (TInt i = 0; i < iCount; ++i)
		{
		const TInt t = TileOf(iGroups[i]);
		if (t >= 0 && iGroups[i].iTarget && !tile[t])
			{
			tile[t] = &iGroups[i];
			++tiles;
			}
		}
	const TBool tiling = (tiles == KMaxTiles);
	const MWsWindowTreeNode* focus = NULL;		// the front tile has the focus
	for (TInt o = 0; o < iOrderCount && !focus; ++o)
		if (TGroup* g = Group(iOrder[o]))
			if (TileOf(*g) >= 0 && g->iSource)
				focus = g->iNode;
	const TSize size = iDevice->SizeInPixels();		// the current screen mode's (the UI turns; the targets do not)
	const TRect screen(size);
	TRegionFix<1> all(screen);
	Next()->Begin(&all);
	iNextGc->SetClippingRegion(all);
	iNextGc->SetDrawMode(MWsGraphicsContext::EDrawModeWriteAlpha);
	iNextGc->SetBrushStyle(MWsGraphicsContext::ESolidBrush);
	iNextGc->SetBrushColor(TRgb(0, 0, 0, 255));
	iNextGc->Clear();
	iNextGc->SetDrawMode(MWsGraphicsContext::EDrawModePEN);
	iSources->DrawResource(iRestSource, screen, screen);
	for (TInt o = iOrderCount - 1; o >= 0; --o)
		{
		TGroup* g = Group(iOrder[o]);
		if (!g || !g->iSource)
			continue;
		const TInt t = TileOf(*g);
		if (tiling && t >= 0)
			{
			// a tile: half the width less a margin, the screen's aspect
			const TInt w = size.iWidth / 2 - 16;
			const TInt h = w * size.iHeight / size.iWidth;
			const TPoint tl(8 + t * (size.iWidth / 2), (size.iHeight - h) / 2);
			const TRect r(tl, TSize(w, h));
			iNextGc->SetDrawMode(MWsGraphicsContext::EDrawModeWriteAlpha);
			iNextGc->SetBrushStyle(MWsGraphicsContext::ENullBrush);
			iNextGc->SetPenStyle(MWsGraphicsContext::ESolidPen);
			iNextGc->SetPenSize(TSize(2, 2));
			iNextGc->SetPenColor(g->iNode == focus ? TRgb(0x40, 0xc0, 0xff) : TRgb(0x60, 0x60, 0x60));	// the front one has the focus
			TRect frame(r);
			frame.Grow(3, 3);
			iNextGc->DrawRect(frame);
			iSources->DrawResource(g->iSource, r, screen);
			iNextGc->SetDrawMode(MWsGraphicsContext::EDrawModePEN);
			}
		else
			iSources->DrawResource(g->iSource, screen, screen);
		}
	iNextGc->ResetClippingRegion();
	if (!(iComposed++ & 0x3f))
		RDebug::Printf("PH: [phecomp] composed #%u: %d groups, tiling %d", iComposed, iOrderCount, tiling);
	Next()->End(aFinished);
	}

void CPheCompStage::End(TRequestStatus* aFinished)
	{
	if (iCompose)
		{
		iProxy.iTarget = iRest->DirectGdiGc();
		Compose(aFinished);
		}
	else
		Next()->End(aFinished);
	++iFrames;
	const TUint32 now = User::NTickCount();
	if (now - iLastSummary >= 1000)
		Summary();
	}

// once a second while something draws: frames and the redraws of each group since the last one
void CPheCompStage::Summary()
	{
	iLastSummary = User::NTickCount();
	TInt total = 0;
	for (TInt i = 0; i < iCount; ++i)
		total += iGroups[i].iRedraws;
	if (!total && iFrames == iFramesLogged)
		return;
	TBuf8<200> line;
	for (TInt i = 0; i < iCount && line.Length() < 150; ++i)
		if (iGroups[i].iRedraws)
			{
			line.AppendFormat(_L8(" %d:%d"), iGroups[i].iId, iGroups[i].iRedraws);
			iGroups[i].iRedraws = 0;
			}
	RDebug::Printf("PH: [phecomp] %u frames, redraws per group (id:n)%S", iFrames - iFramesLogged, &line);
	iFramesLogged = iFrames;
	}

const MWsWindowTreeNode* CPheCompStage::GroupOf(const MWsWindowTreeNode& aNode)
	{
	const MWsWindowTreeNode* n = &aNode;
	while (n && n->NodeType() != MWsWindowTreeNode::EWinTreeNodeGroup)
		n = n->ParentNode();
	return n;
	}

TGroup* CPheCompStage::Group(const MWsWindowTreeNode* aNode)
	{
	if (!aNode)
		return NULL;
	for (TInt i = 0; i < iCount; ++i)
		if (iGroups[i].iNode == aNode)
			return &iGroups[i];
	return NULL;
	}

static void NameOf(const MWsWindowTreeNode& aNode, TDes8& aName)
	{
	aName.Zero();
	const MWsWindowGroup* g = aNode.WindowGroup();
	if (!g)
		return;
	const TPtrC n = g->Name();
	for (TInt i = 0; i < n.Length() && aName.Length() < aName.MaxLength(); ++i)
		aName.Append(n[i] >= 32 && n[i] < 127 ? (TUint8)n[i] : '.');
	}

void CPheCompStage::NodeCreated(const MWsWindowTreeNode& aNode, const MWsWindowTreeNode* aParent)
	{
	if (aNode.NodeType() == MWsWindowTreeNode::EWinTreeNodeGroup)
		{
		if (iCount == KMaxGroups)
			{
			RDebug::Printf("PH: [phecomp] more than %d window groups: not tracked", KMaxGroups);
			return;
			}
		TGroup& g = iGroups[iCount++];
		g.iNode = &aNode;
		g.iId = aNode.WindowGroup() ? aNode.WindowGroup()->Identifier() : 0;
		NameOf(aNode, g.iName);
		g.iRedraws = 0;
		g.iBounds = TRect();
		g.iTarget = NULL;
		g.iSource = NULL;
		SetUid(g);
		if (iOrderCount < KMaxGroups)
			iOrder[iOrderCount++] = &aNode;
		RDebug::Printf("PH: [phecomp] group +%d '%S' (%d groups)", g.iId, &g.iName, iCount);
		}
	else if (aNode.NodeType() == MWsWindowTreeNode::EWinTreeNodeClient && aParent
			 && aParent->NodeType() == MWsWindowTreeNode::EWinTreeNodeGroup)
		{
		TGroup* g = Group(aParent);
		const MWsWindow* w = aNode.Window();
		if (g && w)
			{
			const TRect r = w->AbsRect();
			RDebug::Printf("PH: [phecomp] group %d: top window %08x (%d,%d %dx%d)", g->iId, w->Handle(), r.iTl.iX, r.iTl.iY,
						   r.Width(), r.Height());
			}
		}
	}

void CPheCompStage::NodeReleased(const MWsWindowTreeNode& aNode)
	{
	if (iDrawing == &aNode)
		iDrawing = NULL;
	if (aNode.NodeType() != MWsWindowTreeNode::EWinTreeNodeGroup)
		return;
	for (TInt i = 0; i < iCount; ++i)
		if (iGroups[i].iNode == &aNode)
			{
			RDebug::Printf("PH: [phecomp] group -%d '%S'", iGroups[i].iId, &iGroups[i].iName);
			DropTarget(iGroups[i].iTarget, iGroups[i].iSource);
			for (TInt o = 0; o < iOrderCount; ++o)
				if (iOrder[o] == &aNode)
					{
					Mem::Move(&iOrder[o], &iOrder[o + 1], (iOrderCount - o - 1) * sizeof(iOrder[0]));
					--iOrderCount;
					break;
					}
			iGroups[i] = iGroups[--iCount];
			return;
			}
	}

void CPheCompStage::NodeActivated(const MWsWindowTreeNode& aNode)
	{
	if (aNode.NodeType() != MWsWindowTreeNode::EWinTreeNodeClient)
		return;
	const MWsWindowTreeNode* p = aNode.ParentNode();
	if (p && p->NodeType() == MWsWindowTreeNode::EWinTreeNodeGroup)
		if (TGroup* g = Group(p))
			{
			// the group's name is often set after it was created (CEikonEnv: the app's UID + caption)
			// log on a name change only: every top-level window of an app (CBA, status pane, dialogs) activates
			TBuf8<40> name;
			NameOf(*p, name);
			if (name != g->iName)
				{
				g->iName = name;
				SetUid(*g);
				RDebug::Printf("PH: [phecomp] group %d named '%S'", g->iId, &g->iName);
				}
			}
	}

void CPheCompStage::NodeExtentChanged(const MWsWindowTreeNode& aNode, const TRect& aRect)
	{
	const MWsWindowTreeNode* p = aNode.ParentNode();
	if (aNode.NodeType() != MWsWindowTreeNode::EWinTreeNodeClient || !p || p->NodeType() != MWsWindowTreeNode::EWinTreeNodeGroup)
		return;
	if (TGroup* g = Group(p))
		{
		if (g->iBounds.IsEmpty())
			g->iBounds = aRect;
		else
			g->iBounds.BoundingRect(aRect);
		RDebug::Printf("PH: [phecomp] group %d: top window at (%d,%d %dx%d)", g->iId, aRect.iTl.iX, aRect.iTl.iY, aRect.Width(),
					   aRect.Height());
		}
	}

void CPheCompStage::SiblingOrderChanged(const MWsWindowTreeNode& aNode, TInt aNewPos)
	{
	if (aNode.NodeType() != MWsWindowTreeNode::EWinTreeNodeGroup)
		return;
	TGroup* g = Group(&aNode);
	if (!g)
		return;
	RDebug::Printf("PH: [phecomp] group %d '%S' -> position %d", g->iId, &g->iName, aNewPos);
	// keep iOrder front to back: take the group out, put it back at its new position
	TInt o = 0;
	while (o < iOrderCount && iOrder[o] != &aNode)
		++o;
	if (o == iOrderCount)
		return;
	Mem::Move(&iOrder[o], &iOrder[o + 1], (iOrderCount - o - 1) * sizeof(iOrder[0]));
	const TInt n = Min(Max(aNewPos, 0), iOrderCount - 1);
	Mem::Move(&iOrder[n + 1], &iOrder[n], (iOrderCount - 1 - n) * sizeof(iOrder[0]));
	iOrder[n] = &aNode;
	}

void CPheCompStage::FlagChanged(const MWsWindowTreeNode& aNode, TFlags aFlag, TBool aNewValue)
	{
	if (aFlag != EVisible || aNode.NodeType() != MWsWindowTreeNode::EWinTreeNodeClient)
		return;
	const MWsWindowTreeNode* p = aNode.ParentNode();
	if (p && p->NodeType() == MWsWindowTreeNode::EWinTreeNodeGroup)
		if (TGroup* g = Group(p))
			RDebug::Printf("PH: [phecomp] group %d: top window %s", g->iId, aNewValue ? "shown" : "hidden");
	}

void CPheCompStage::ElementAdded(const MWsWindowTreeNode& aNode, const MWsElement&)
	{
	if (TGroup* g = Group(GroupOf(aNode)))
		RDebug::Printf("PH: [phecomp] group %d: a surface element (background surface)", g->iId);
	}

void CPheCompStage::WindowGroupChained(const MWsWindowTreeNode& aParent, const MWsWindowTreeNode& aChild)
	{
	TGroup* p = Group(&aParent);
	TGroup* c = Group(&aChild);
	if (p && c)
		RDebug::Printf("PH: [phecomp] group %d chained under %d", c->iId, p->iId);
	}

void CPheCompStage::WindowRedrawStart(const MWsWindowTreeNode& aNode, const TRegion&)
	{
	iDrawing = &aNode;
	TGroup* g = Group(GroupOf(aNode));
	if (g)
		++g->iRedraws;
	if (!iCompose)
		return;
	// the window server's drawing of this window goes to its group's target (made at the first redraw)
	if (g && !g->iTarget && MakeTarget(g->iTarget, g->iSource) == KErrNone)
		RDebug::Printf("PH: [phecomp] group %d '%S': own target", g->iId, &g->iName);
	iProxy.iTarget = (g && g->iTarget) ? g->iTarget->DirectGdiGc() : iRest->DirectGdiGc();
	}

void CPheCompStage::WindowRedrawEnd(const MWsWindowTreeNode&)
	{
	iDrawing = NULL;
	if (iCompose)
		iProxy.iTarget = iRest->DirectGdiGc();
	}

// "<nn>.<uid hex>.<caption>" (CEikonEnv's window group name) -> the uid
void CPheCompStage::SetUid(TGroup& aGroup)
	{
	aGroup.iUid = 0;
	const TInt dot = aGroup.iName.Locate('.');
	if (dot < 0)
		return;
	TLex8 lex(aGroup.iName.Mid(dot + 1));
	TUint32 uid;
	if (lex.Val(uid, EHex) == KErrNone && lex.Peek() == '.')
		aGroup.iUid = uid;
	}

// ---- the plug-in: the factory the window server finds by the wsini name "phecomp" ----

_LIT(KPheCompName, "phecomp");

NONSHARABLE_CLASS(CPheCompFactory) : public CWsPlugin, public MWsRenderStageFactory
	{
public:
	enum { EImplUid = 0xE7017C01 };
	static CPheCompFactory* CreateL() { return new(ELeave) CPheCompFactory; }
	void ConstructL(MWsGraphicDrawerEnvironment& aEnv, const TDesC8&) { BaseConstructL(aEnv); }
	const TDesC& PluginName() const { return KPheCompName; }
	TAny* ResolveObjectInterface(TUint aTypeId)
		{ return aTypeId == KMWsRenderStageFactory ? static_cast<MWsRenderStageFactory*>(this) : NULL; }
	CWsRenderStage* CreateFinalRenderStageL(MWsScreen*, MWsScreenRedraw*, TInt)
		{
		RDebug::Printf("PH: [phecomp] cannot be the last render stage (it passes everything on)");
		User::Leave(KErrNotSupported);
		return NULL;
		}
	CWsRenderStage* CreateRenderStageL(MWsScreen*, MWsScreenRedraw*, CWsRenderStage* aNext)
		{ return CPheCompStage::NewL(aNext, Env().ObjectInterface<MWsIniFile>()); }
	};

} // namespace

const TImplementationProxy KImplementationTable[] =
	{
	IMPLEMENTATION_PROXY_ENTRY(CPheCompFactory::EImplUid, CPheCompFactory::CreateL),
	};

EXPORT_C const TImplementationProxy* ImplementationGroupProxy(TInt& aTableCount)
	{
	aTableCount = sizeof(KImplementationTable) / sizeof(TImplementationProxy);
	return KImplementationTable;
	}
