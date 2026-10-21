/*
* Copyright (c) 2008 Nokia Corporation and/or its subsidiary(-ies).
* All rights reserved.
* This component and the accompanying materials are made available
* under the terms of the Eclipse Public License v2.0, available at
* https://www.eclipse.org/legal/epl-2.0/ (relicensed from v1.0 under
* section 7 of the Eclipse Public License v1.0).
* This Source Code may also be made available under the following Secondary
* Licenses when the conditions for such availability set forth in the Eclipse
* Public License, v. 2.0 are satisfied: GNU General Public License, version 2
* or later.
*
* Initial Contributors:
* Nokia Corporation - initial contribution.
*
* Contributors:
* The Phorepoc Authors
*
* Description:  Gesture helper implementation
*
*/

#include "gesturerecogniser.h"

#include <e32math.h>

#include "gesturedefs.h"
#include "gestureobserver.h"
#include "pointarray.h"
#include "utils.h"

#include <w32std.h>  // pointerevents

using namespace GestureHelper;

/**
 * Vector class (math)
 */
NONSHARABLE_CLASS( TVector )
    {
public:
    /** 
     * Constructor
     * @param aFrom starting point of the vector
     * @param aTo ending point of the vector
     */
    TVector( const TPoint& aFrom, const TPoint& aTo )
            : iX( aTo.iX - aFrom.iX ),
              iY( aTo.iY - aFrom.iY )
        {
        }
        
    /** @return angle of the vector */
    TReal Angle() const
        {
        TReal angle = 0;   
        TReal length = Length();
        if ( length != 0 )
            {
            Math::ACos( angle, iX / Length() );
            if ( iY < 0 )
                {
                angle = 2 * KPi - angle;
                }
            }
        return Degrees( angle ); 
        }
        
    /** @return length of the vector */
    TReal Length() const
        {
        TReal length = 0;
        Math::Sqrt( length, iX * iX + iY * iY );
        return length;
        }

private:
    /** @return radians in degrees */
    inline TReal Degrees( TReal aRadians ) const
        {
        return aRadians * 180 / KPi;
        }

public:
    /// x coordinate that represent the vector
    TReal iX;
    /// y coordinate that represent the vector
    TReal iY;
    };

inline TPoint LastPoint( const TPointArray& aPoints ) 
    {
    __ASSERT_DEBUG( aPoints.Count() > 0, Panic( EGesturePanicIllegalLogic ) );
    return aPoints[ aPoints.Count() - 1 ] ;
    }

/** 
 * @return Length of the gesture in points
 */
inline TReal GestureLength( const TPointArray& aPoints )
    {
    return TVector( aPoints[0], LastPoint( aPoints ) ).Length();
    }

/** 
 * @return ETrue if aAngleUnderTest is almost aAngle
 *         Closeness of the angles is controlled by KAngleTolerance
 */
static TBool IsNear( TReal aAngleUnderTest, TReal aAngle )
    {
    return aAngle - KAngleTolerance <= aAngleUnderTest && 
           aAngleUnderTest <= aAngle + KAngleTolerance;
    }
    
/**
 * @return the angle as a direction flags of TGesture
 */
inline TGestureCode Direction( TReal aAngle ) 
    {
    TGestureCode direction = EGestureUnknown;

    if ( IsNear( aAngle, 90.0 ) )
        {
        direction = EGestureSwipeDown;
        }
    else if ( IsNear( aAngle, 180.0 ) )
        {
        direction = EGestureSwipeLeft;
        }
    else if ( IsNear( aAngle, 270.0 ) )
        {
        direction = EGestureSwipeUp;
        }
    else if ( 360.0 - KAngleTolerance <= aAngle || aAngle <= KAngleTolerance )
        {
        direction = EGestureSwipeRight;
        }
    else // for lint warning
        {
        // unknown angle
        }
    
    return direction;
    }

/** @return direction between points */
inline TGestureCode Direction( const TPoint& aFromPoint, const TPoint& aToPoint )
    {
    return Direction( TVector( aFromPoint, aToPoint ).Angle() );
    }

// function type to get a point in the point array
typedef TPoint (TPointArray::*PointByIndexFunc)( TInt aIndex ) const;    

/// @return latest point outside tolerance area or KErrNotFound if not point outside it
TInt LatestCertainPointIndex( const TPointArray& aPoints, PointByIndexFunc aPointByIndex, TRect aToleranceRect )   
    {
    int i = aPoints.Count();
    // Find out the first point from the end of the array  
    // that is not contained in the tolerance rect.
    while( 0 <= --i )
        {
        // if the point does not belong inside tolerance rect, it is the first point
        // outside the rect
        if( !aToleranceRect.Contains( (aPoints.*aPointByIndex)(i) ) )
            {
            break;
            }
        }
    return i;
    }
    
/** @return last direction of dragging */
inline TGestureCode LastDirection( const TPointArray& aPoints, MGestureEvent::TAxis aRelevantAxis ) 
    {
    __ASSERT_DEBUG( aPoints.Count() > 0, Panic( EGesturePanicIllegalLogic ) );
    
    TRect toleranceRect = ToleranceRect( (aPoints.TPointArray::operator[])(aPoints.Count() - 1), aRelevantAxis );
    
    TInt latestPointIndex = LatestCertainPointIndex( aPoints, &TPointArray::operator[],toleranceRect  );  // PHOREPOC-DELTA (GCC): &Class::member
    if ( KErrNotFound != latestPointIndex )
        {
        return Direction( aPoints[latestPointIndex], LastPoint( aPoints ) );
        }
    // no points were outside the rect, and hence the direction is unknown
    return EGestureUnknown;
    }
    
/**
 * @return ETrue if points for a tap event
 */
inline TBool IsTap( const TPointArray& aPoints )
    {
    __ASSERT_DEBUG( aPoints.Count() > 0, Panic( EGesturePanicIllegalLogic ) );
    
    TRect toleranceRect = ToleranceRect( (aPoints.TPointArray::Raw)(0) );
    
    return KErrNotFound == LatestCertainPointIndex( aPoints, &TPointArray::Raw,toleranceRect );  // PHOREPOC-DELTA (GCC)
    }  
    
// ----------------------------------------------------------------------------
// Return gesture code of a gesture formed by a sequence of points
// ----------------------------------------------------------------------------
//
TGestureCode TGestureRecogniser::GestureCode( const TPointArray& aPoints, MGestureEvent::TAxis aRelevantAxis ) const
    {
    __ASSERT_DEBUG( aPoints.Count() > 0, Panic( EGesturePanicIllegalLogic ) );
    
    // tap needs to be treated separately, because recognising that needs to consider
    // raw points (and not points in which x or y axis has been filtered out)
    if ( IsTap( aPoints ) )
        {
        return EGestureTap;
        }
    
	return LastDirection( aPoints, aRelevantAxis ); 
    }

// ----------------------------------------------------------------------------
// Return gesture code of a gesture formed by a sequence of points
// ----------------------------------------------------------------------------
//
TGestureCode TGestureRecogniser::MultiTouchGestureCode( const TPointArray& aPoints, const TPointArray& aSecondaryPoints, 
        TInt aPreviousDistance,  TBool aIsFirstPinch ) const
    {
    __ASSERT_DEBUG( aPoints.Count() > 0, Panic( EGesturePanicIllegalLogic ) );
    __ASSERT_DEBUG( aSecondaryPoints.Count() > 0, Panic( EGesturePanicIllegalLogic ) );
    __ASSERT_DEBUG( aPreviousDistance > 0, Panic( EGesturePanicIllegalLogic ) );
    
    TInt currentDistance = TVector( LastPoint( aPoints ), LastPoint( aSecondaryPoints ) ).Length();
 
    // currently gesture tolerance is used as reference to identify the pinch
    // and it is same for first time recognition and also for pinch thereon 
    // This can be changed to an independent constant if required  
    // This is now implemeted. We have different tolerance for intial pinch and pinhc theron
    
    // We need to have different tolerance at the time of changing direction.
    // When user is pinching in and then starts pinching out the tolerance should be different.
	// This is implemented in CGesture class. aIsFirstPinch will take care of all this 
    TInt pinchTolerance = aIsFirstPinch ? KInitialPinchTolerance : KPinchTolerance;
    if (Abs( currentDistance - aPreviousDistance ) >= pinchTolerance )
        {
        return EGesturePinch;
        }    
    return EGestureUnknown;
    }

// ----------------------------------------------------------------------------
// Validate the drag formed by a sequence of points. If all the points are 
// within the tolerance rect then the gesture is not recognised.
// ----------------------------------------------------------------------------
//
TGestureCode TGestureRecogniser::ValidateDrag( const TPointArray& aPoints, MGestureEvent::TAxis aRelevantAxis ) const
    {
    __ASSERT_DEBUG( aPoints.Count() > 0, Panic( EGesturePanicIllegalLogic ) );    
   
    TRect toleranceRect = ToleranceRect( (aPoints.TPointArray::operator[])(aPoints.Count() - 1), aRelevantAxis );
    
    TInt latestPointIndex = LatestCertainPointIndex( aPoints, &TPointArray::operator[],toleranceRect  );  // PHOREPOC-DELTA (GCC): &Class::member
    if ( KErrNotFound != latestPointIndex )
        {
        return EGestureDrag;
        }
    // no points were outside the rect, and hence the direction is unknown
    return EGestureUnknown;
    }

// ----------------------------------------------------------------------------
// Return the length between the given two points
// ----------------------------------------------------------------------------
//
TInt TGestureRecogniser::Length( const TPoint aPoint, const TPoint aSecondaryPoint ) const
    {
    return TVector(aPoint,aSecondaryPoint).Length();
    }

// ----------------------------------------------------------------------------
// Return pointer number fetched from the given event
// ----------------------------------------------------------------------------
//
TInt TGestureRecogniser::PointerNumber( const TPointerEvent& aEvent ) 
    {
    if( aEvent.IsAdvancedPointerEvent() )
        {
        return aEvent.AdvancedPointerEvent()->PointerNumber();
        }
    return 0;
    }
// end of file

