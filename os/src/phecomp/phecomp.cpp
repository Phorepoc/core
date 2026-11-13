// Copyright The Phorepoc Authors
// SPDX-License-Identifier: BSD-2-Clause

// phecomp.cpp -- OURS: Phorepoc's window compositor as a render stage of the NGA window server
// (docs: core os/src/phecomp/README.md). v0, step "trace": a PASS-THROUGH stage that sits first in the
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

namespace {

const TInt KMaxGroups = 32;

// what the stage knows of one window group
struct TGroup
	{
	const MWsWindowTreeNode* iNode;
	TInt iId;						// the window group identifier (RWindowGroup::Identifier)
	TBuf8<40> iName;				// the start of its name (the app's caption follows the UID)
	TInt iRedraws;					// window redraw brackets since the last summary
	TRect iBounds;					// the union of its top-level windows' extents
	};

NONSHARABLE_CLASS(CPheCompStage) : public CWsRenderStage, public MWsWindowTreeObserver, public MWsDrawAnnotationObserver
	{
public:
	static CPheCompStage* NewL(CWsRenderStage* aNext);
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
	TGroup* Group(const MWsWindowTreeNode* aNode);
	static const MWsWindowTreeNode* GroupOf(const MWsWindowTreeNode& aNode);
	void Summary();
private:
	TGroup iGroups[KMaxGroups];
	TInt iCount;
	const MWsWindowTreeNode* iDrawing;		// the window inside a redraw bracket (NULL outside)
	TUint32 iFrames, iFramesLogged;
	TUint32 iLastSummary;					// User::NTickCount of the last summary
	};

CPheCompStage* CPheCompStage::NewL(CWsRenderStage* aNext)
	{
	CPheCompStage* self = new(ELeave) CPheCompStage;
	CleanupStack::PushL(self);
	self->BaseConstructL();
	self->SetNext(aNext);
	CleanupStack::Pop(self);
	RDebug::Printf("PH: [phecomp] trace stage up (pass-through)");
	return self;
	}

CPheCompStage::~CPheCompStage()
	{
	}

TAny* CPheCompStage::ResolveObjectInterface(TUint aTypeId)
	{
	switch (aTypeId)
		{
	case MWsWindowTreeObserver::EWsObjectInterfaceId:
		return static_cast<MWsWindowTreeObserver*>(this);
	case MWsDrawAnnotationObserver::EWsObjectInterfaceId:
		return static_cast<MWsDrawAnnotationObserver*>(this);
	default:
		return CWsRenderStage::ResolveObjectInterface(aTypeId);	// the next stage (the GC, the scene, the display ...)
		}
	}

void CPheCompStage::Begin(const TRegion* aRegion)
	{
	Next()->Begin(aRegion);
	}

void CPheCompStage::End(TRequestStatus* aFinished)
	{
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
			NameOf(*p, g->iName);
			RDebug::Printf("PH: [phecomp] group %d '%S': top window active", g->iId, &g->iName);
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
	if (TGroup* g = Group(&aNode))
		RDebug::Printf("PH: [phecomp] group %d '%S' -> position %d", g->iId, &g->iName, aNewPos);
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
	if (TGroup* g = Group(GroupOf(aNode)))
		++g->iRedraws;
	}

void CPheCompStage::WindowRedrawEnd(const MWsWindowTreeNode&)
	{
	iDrawing = NULL;
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
		{ return CPheCompStage::NewL(aNext); }
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
