// Copyright (c) 2008-2009 Nokia Corporation and/or its subsidiary(-ies).
// All rights reserved.
// This component and the accompanying materials are made available
// under the terms of "Eclipse Public License v1.0"
// which accompanies this distribution, and is available
// at the URL "http://www.eclipse.org/legal/epl-v10.html".
//
// Initial Contributors:
// Nokia Corporation - initial contribution.
//
// Contributors:
//
// Description:
// OURS copy of nonnga/CLIENT/advancedpointerevent.cpp (copy ctor + operator= made real, below).
// Contains dummy implementations for exported methods implemented in NGA version
// of this file which share def files with WSERV NONNGA.
// 
//

#include <w32std.h>
#include "advancedpointerevent.inl"

EXPORT_C TInt TAdvancedPointerEvent::DoGetPointerNumber() const
/** Dummy implementation in order to preserve compatibility with WSERV NGA.
    @internalComponent */ 
	{
	ASSERT(0);
	return 0;
	}

EXPORT_C TInt TAdvancedPointerEvent::DoGetProximity() const
/** Dummy implementation in order to preserve compatibility with WSERV NGA.
    @internalComponent */ 
	{
	ASSERT(0);
	return 0;
	}

EXPORT_C TInt TAdvancedPointerEvent::DoGetPressure() const
/** Dummy implementation in order to preserve compatibility with WSERV NGA.
    @internalComponent */ 
	{
	ASSERT(0);
	return 0;
	}

EXPORT_C TInt TAdvancedPointerEvent::DoGetProximityAndPressure() const
/** Dummy implementation in order to preserve compatibility with WSERV NGA.
    @internalComponent */ 
	{
	ASSERT(0);
	return 0;
	}

EXPORT_C void TWsEvent::InitAdvancedPointerEvent(TPointerEvent::TType /*aType*/, TUint /*aModifiers*/, const TPoint3D &/*aPoint3D*/, TUint8 /*aPointerNumber*/)
/** Dummy implementation in order to preserve compatibility with WSERV NGA.
    @internalComponent */ 
	{ 
	ASSERT(0);
	}

EXPORT_C void TWsEvent::SetPointerNumber(TUint8 /*aPointerNumber*/)
/** Dummy implementation in order to preserve compatibility with WSERV NGA.
    @internalComponent */ 
	{
	ASSERT(0);
	}

EXPORT_C void TWsEvent::SetPointerZ(TInt /*aZ*/)
/** Dummy implementation in order to preserve compatibility with WSERV NGA.
    @internalComponent */ 
	{
	ASSERT(0);
	}

// OURS (Phorepoc): the copy constructor and operator= do what the NGA client's do (nga/CLIENT/advancedpointerevent.cpp,
// Copy()) instead of the non-NGA dummies. Non-NGA WSERV never sends advanced events, but clients still copy every
// pointer event through this type -- Avkon's touch gesture framework does `TAdvancedPointerEvent e(*aEvent.Pointer())`
// in MonitorWsMessage -- and the dummy left that copy uninitialised: no taps or long taps anywhere
// (2026-10-04).
EXPORT_C TAdvancedPointerEvent::TAdvancedPointerEvent(const TAdvancedPointerEvent& aPointerEvent) // Copy Constructor
	{
	Copy(aPointerEvent);
	}

EXPORT_C TAdvancedPointerEvent& TAdvancedPointerEvent::operator=(const TAdvancedPointerEvent& aPointerEvent) // Operator = overload
	{
	if(this != &aPointerEvent)
		{
		Copy(aPointerEvent);
		}
	return *this;
	}
