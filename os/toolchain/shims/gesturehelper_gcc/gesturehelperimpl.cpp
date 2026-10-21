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

#include "gesturehelperimpl.h"

#include <alf/alfevent.h>
#include <e32base.h>
#include <w32std.h>

#include "gesture.h"
#include "gesturedefs.h"

#include "pointercapturer.h"

using namespace GestureHelper;

namespace GestureHelper
{

/// type of function in gesture helper to be called by the timer
/// when timer triggers
typedef void (CGestureHelperImpl::*CallbackFunctionL)();

NONSHARABLE_CLASS( CCallbackTimer ) : public CTimer
    {
public:
    /** Two-phase constructor */
    static CCallbackTimer* NewL( CGestureHelperImpl& aHelper, 
            CallbackFunctionL aCallbackFunctionL, TInt aDelay, TBool aIsEnabled )
        {
        CCallbackTimer* self = new ( ELeave ) CCallbackTimer( aHelper, 
            aCallbackFunctionL, aDelay, aIsEnabled );
        CleanupStack::PushL( self );
        self->ConstructL(); // construct base class
        CActiveScheduler::Add( self );
        CleanupStack::Pop( self );
        return self;
        }
        
    /** Destructor */
    ~CCallbackTimer()
        {
        Cancel();
        }
        
    /** Set whether sending holding events is currently enabled */
    void SetEnabled( TBool aEnabled )
        {
        iIsEnabled = aEnabled;
        // cancel in case hold timer is already running
        Cancel();
        }
        
    /** @return whether sending holding events is currently enabled */
    TBool IsEnabled() const
        {
        return iIsEnabled;
        }
        
    /** Start the timer. Calls CGestureHelperImpl::StartHoldingL upon completion */
    void Start()
        {
        // if sending hold events is disabled, do not ever start the hold timer, and 
        // hence hold events will never be triggered
        if ( iIsEnabled ) 
            {
            Cancel();
            After( iDelay );
            }
        }    
    
private:    
    /** Constructor */
    CCallbackTimer( CGestureHelperImpl& aHelper,
        CallbackFunctionL aCallbackFunctionL, TInt aDelay, TBool aIsEnabled )
            : CTimer( EPriorityUserInput - 1 ), // give higher priority to new pointer events with - 1
                iHelper( aHelper ), iCallbackFunctionL( aCallbackFunctionL ), 
                    iDelay( aDelay ), iIsEnabled( aIsEnabled ) 
        {
        }
        
    void RunL() // From CActive
        {
        (iHelper.*iCallbackFunctionL)();
        }

private:
    /// helper object that will be called back when timer is triggered
    CGestureHelperImpl& iHelper;
    /// Function in the iHelper object call 
    CallbackFunctionL iCallbackFunctionL;
    /// How long a time to wait befor calling back after Start()
    TInt iDelay;
    /// whether sending holding events is currently enabled
    TBool iIsEnabled;
    };

} // namespace GestureHelper

/** 
 * @return position from event. Use this instead of using aEvent direction to
 *         avoid accidentally using TPointerEvent::iPosition
 */
inline TPoint Position( const TPointerEvent& aEvent )
    {
    // use parent position, since the capturer is using full screen area,
    // and because the (Alfred) drag events are not local to visual even when
    // coming from the client
    return aEvent.iParentPosition;
    }

// ----------------------------------------------------------------------------
// Two-phase constructor
// ----------------------------------------------------------------------------
//
CGestureHelperImpl* CGestureHelperImpl::NewL( MGestureObserver& aObserver )
    {
    CGestureHelperImpl* self = new ( ELeave ) CGestureHelperImpl( );
    CleanupStack::PushL( self );
    self->iDoubleTapTimer = CCallbackTimer::NewL( *self, &CGestureHelperImpl::EmitFirstTapEventL,  // PHOREPOC-DELTA (GCC): &Class::member 
        KMaxDoubleTapDuration, EFalse ); // double tap is disabled by default
    self->iHoldingTimer = CCallbackTimer::NewL( *self, &CGestureHelperImpl::StartHoldingL,  // PHOREPOC-DELTA (GCC) 
        KHoldDuration, ETrue ); // holding is enabled by default
    self->iGesture = new ( ELeave ) CGesture();
    self->iUnusedGesture = new ( ELeave ) CGesture();
    self->iPointerCapturer = CPointerCapturer::NewL();
    self->iPointerCount = 0;
    self->iObserver = &aObserver;
    CleanupStack::Pop( self );
    return self;
    }

// ----------------------------------------------------------------------------
// Destructor
// ----------------------------------------------------------------------------
//
CGestureHelperImpl::~CGestureHelperImpl()
    {
    delete iDoubleTapTimer;
    delete iHoldingTimer;
    delete iGesture;
    delete iPreviousTapGesture;
    delete iUnusedGesture;
    delete iPointerCapturer;
    }
    
// ----------------------------------------------------------------------------
// SetHoldingEnabled
// ----------------------------------------------------------------------------
//
void CGestureHelperImpl::SetHoldingEnabled( TBool aEnabled )
    {
    iHoldingTimer->SetEnabled( aEnabled );
    }

// ----------------------------------------------------------------------------
// IsHoldingEnabled
// ----------------------------------------------------------------------------
//
TBool CGestureHelperImpl::IsHoldingEnabled() const
    {
    return iHoldingTimer->IsEnabled();
    }

// ----------------------------------------------------------------------------
// SetHoldingEnabled
// ----------------------------------------------------------------------------
//
void CGestureHelperImpl::SetDoubleTapEnabled( TBool aEnabled )
    {
    iDoubleTapTimer->SetEnabled( aEnabled );
    }

// ----------------------------------------------------------------------------
// IsHoldingEnabled
// ----------------------------------------------------------------------------
//
TBool CGestureHelperImpl::IsDoubleTapEnabled() const
    {
    return iDoubleTapTimer->IsEnabled();
    }
    
// ----------------------------------------------------------------------------
// InitAlfredPointerEventCaptureL
// ----------------------------------------------------------------------------
//
void CGestureHelperImpl::InitAlfredPointerCaptureL( CAlfEnv& aEnv, 
        CAlfDisplay& aDisplay, TInt aFreeControlGroupId )
    {
    iPointerCapturer->InitForAlfredL(*this, aEnv, aDisplay, aFreeControlGroupId );
    }

// ----------------------------------------------------------------------------
// Reset state
// ----------------------------------------------------------------------------
//
void CGestureHelperImpl::Reset()
    {
    iHoldingTimer->Cancel();
    iGesture->Reset();
    iPointerCapturer->Stop();
    iPointerCount=0;
    iCurrentPointer = -1;
    }

/** 
 * Helper function that calls Reset on the pointer to CGestureHelperImpl
 */
static void ResetHelper( TAny* aHelper )
    {
    static_cast< CGestureHelperImpl* >( aHelper )->Reset();
    }

// ----------------------------------------------------------------------------
// Handle a pointer event
// ----------------------------------------------------------------------------
//
TBool CGestureHelperImpl::HandlePointerEventL( const TPointerEvent& aEvent )
    {     
    return HandlePointerEventL( aEvent, NULL );
    }

// ----------------------------------------------------------------------------
// OfferEventL
// ----------------------------------------------------------------------------
//
TBool CGestureHelperImpl::OfferEventL( const TAlfEvent& aEvent )
    {
    if ( aEvent.IsPointerEvent() )
        {
        return HandlePointerEventL( aEvent.PointerEvent(), aEvent.Visual() );
        }
    return EFalse;
    }

// ----------------------------------------------------------------------------
// Handle a pointer event
// ----------------------------------------------------------------------------
//
TBool CGestureHelperImpl::HandlePointerEventL( const TPointerEvent& aEvent,
        CAlfVisual* aVisual )
    {  
    TInt pointerNumber = GestureHelper::TGestureRecogniser().PointerNumber( aEvent );

    if( !ValidatePointer( aEvent, pointerNumber ) )
        {
        return EFalse; // don't consume
        }

    switch ( aEvent.iType )
        {
        case TPointerEvent::EButton1Down:            
		    // Error handling for recieveing a button down on the pointer which
            // is already touched down
            // Here the pointer number shouldnot be considered for validtaing.
            // We should consider which pointer is currently touched down
            // It can so happen that user has touched was doing pinch and then released his
            // 1st finger. and starts to do panning with the second finger still touched down.
            // Then again when he touches the first finger, in that case you should be validating these pointers w.r.t
            // the pointer numbers of the last touched finger
             
            // we shouldnot consider pointer number here. If pointer 1 is
            // already touched and dragging and we recieve a down of pointer 0     
            if( iPointerCount == 0 && pointerNumber == 1)
                {
                return EFalse;                
                }        
            else if( iPointerCount == 1 )
                {
                if(iCurrentPointer == pointerNumber )
                    {
                    TRAP_IGNORE( EmitCancelEventL() );
                    Reset();
                    }
                }
            else if( iPointerCount == 2 )
                {
                // This case is similar to reciving a pointer up on the pointer
                // on which the second down is recieved. We reset all the earlier points
                // recieved on this pointer because we assume that some pointer up got
                // missed in between.
            
                // if pointer count is already 2, then reset the array of pointer for 
                // which a down event is recieved, and continue handling in normal way
                // Fix for error crash in photos fullscreen
                // Like above if you reset the pointer array for which the down event
                // is recieved the second time then in thecase of, 0 down, 1 down, 0 down
                // iPoints will be null.
                // Here whenever reseting it to single pointer havndling, always iPoints should have
                // the data. Hence the first parameter should always be true.
                // Fix is iGesture->ResetToLastPoint(pointerNumber != 0,pointerNumber != 0);
                // is changed to iGesture->ResetToLastPoint( ETrue,pointerNumber != 0);
                iPointerCount = 1; 
                iCurrentPointer = pointerNumber == 0 ? 1 : 0;
                iGesture->ResetToLastPoint( ETrue,pointerNumber != 0);
                iGesture->SetSingleTouchActive();
                }

            if(iPointerCount == 0)
                {
                iPointerCount = 1;
                iCurrentPointer = pointerNumber;
                // single touch gesture start
                iGesture->SetSingleTouchActive();
                HandleSinglePointerEventL( aEvent, aVisual );
                }
            else if(iPointerCount == 1)
                {              
                iPointerCount = 2;
                iCurrentPointer = -1;
                // add the last point of the single touch event
                // to first array of gesture
                iGesture->ResetToLastPoint(pointerNumber != 0,ETrue);
                iGesture->SetMultiTouchActive();
                // multi touch gesture start
                HandleMultiplePointerEventL( aEvent, pointerNumber );
                }
            else
                {
                
                }
            break;
            
        case TPointerEvent::EDrag:
            if(iPointerCount == 1)
                {
                if(pointerNumber == iCurrentPointer)
                    {
                    HandleSinglePointerEventL( aEvent, aVisual );
                    }
                else
                    {
                    // only the drags on the current pointer should be considered.
                    return EFalse;
                    }
                
                }
            else if(iPointerCount == 2)
                {
                HandleMultiplePointerEventL( aEvent, pointerNumber );
                }
            else
                {
                // nothing to be done
                }
            break;
            
        case TPointerEvent::EButton1Up:
            if(iPointerCount == 2)
                { 
                // multi touch gesture complete
                HandleMultiplePointerEventL( aEvent, pointerNumber );
                // should the pointer count decrese first n then
                // handling of event or otherwise
                iPointerCount = 1;    
                iCurrentPointer = pointerNumber == 0 ? 1 : 0;
                iGesture->ResetToLastPoint(ETrue,pointerNumber != 0);
                iGesture->SetSingleTouchActive();
                }
            else if( iPointerCount == 1 )
                {
                iPointerCount = 0;
                iCurrentPointer = -1;
                // single touch gesture complete
                HandleSinglePointerEventL( aEvent, aVisual ); 
                }
            else
                {
                // nothing to be done
                }
            break;
            
        default:
            break;
        }
    return ETrue; // consume
    }
// ----------------------------------------------------------------------------
// Handle a pointer event
// ----------------------------------------------------------------------------
//
TBool CGestureHelperImpl::HandleSinglePointerEventL( const TPointerEvent& aEvent,
        CAlfVisual* aVisual )
    {  
    switch ( aEvent.iType )
        {
        case TPointerEvent::EButton1Down:
            // If no up event was received during previous gesture, cancel 
            // previous event and reset state
            if ( !IsIdle() )
                {
                // ambiguous what is the right thing when "cancel" event leaves
                // and "start" does not. Leaving for cancel *after* "start" could 
                // be unexpected to client, as client would have handled start 
                // event successfully. Assume that leaving upon cancellation 
                // can be ignored.
                TRAP_IGNORE( EmitCancelEventL() );
                Reset();  
                }
            // as long as down event of a double tap comes within the double 
            // tap timeout, it does not matter how long the user keeps the finger
            // pressed for the gesture to be a double tap. Therefore, cancel
            // the timeout, as it is no longer relevant. (Of course, this call
            // will only do something if the timer is actually running, which
            // is only if received a tap event very recently.)
            iDoubleTapTimer->Cancel();
            // adding the first point implicitly makes the state "not idle"
            AddPointL( aEvent );
            iGesture->SetVisual( aVisual );
            // if pointer capturer leaves, the remaining pointer events will
            // not be captured if stylus is dragged outside the capturing visual
            // an error note will be shown, so the potential problem is irrelevant,
            // assuming client does not (incorrectly) block the leave from reaching 
            // the framework
            iPointerCapturer->StartL();
            // Delay emitting a down event _until_ it is known that this beginning 
            // gesture is _not_ the second tap of a double tap event.
            // iPreviousTapGesture is only non-null if very recently received 
            // a tap event and double tap is enabled. 
            if ( !iPreviousTapGesture )
                {
                EmitEventL( *iGesture );
                }
            // else delay emitting an event, as it might be a double tap 
            // (allow the second tap of a double tap to be anywhere, so don't check
            // for start pos here)
            break;
            
        case TPointerEvent::EDrag:
            // While stylus down, the same event is received repeatedly
            // even if stylus does not move. Filter out by checking if point 
            // is the same as the latest point
            if ( !iGesture->IsLatestPoint( Position( aEvent ) ) )
                {
                AddPointL( aEvent );

                // as long as the starting gesture is seen as a tap, do not emit any
                // drag events
                if ( !iGesture->IsTap() )
                    {
                    // if there is a previous tap gesture, getting drag events means that
                    // the previous gesture is not a double tap. So emit the previous gesture.
                    if ( iPreviousTapGesture )
                        {
                        // this is a second gesture after a tap (double tap is enabled)
                        EmitFirstTapEventL();
                        // emit down event for the current gesture (since its down was delayed, until
                        // it was to be known if the event is a tap. That is known now.)
                        EmitStartEventL( *iGesture );
                        }
                    // restart holding timer every time the current stylus pos changes
                    StartHoldingTimer( aEvent );
                    // emit the drag event to client
                    EmitEventL( *iGesture );
                    }
                // else: do not emit drag events until it is known that the gesture is not a tap
                // (or the second tap of double tap)
                }
            break;

        case TPointerEvent::EButton1Up:
            // reset in case the down event for next gesture is not received for a reason 
            // in client, and instead drag or up events are received. 
            // reset via cleanup stack to ensure Reset is run even if
            // observer leaves
            CleanupStack::PushL( TCleanupItem( &ResetHelper, this ) );
            // if adding of the point fails, notify client with a 
            // cancelled event. It would be wrong to send another
            // gesture code when the up point is not known
            if ( KErrNone == AddPoint( aEvent ) )
                {
                // if the gesture is a tap, the gesture is either the first tap of a _potential_
                // double tap, or the second tap of a double tap
                if ( iDoubleTapTimer->IsEnabled() && iGesture->IsTap() )
                    {
                    __ASSERT_DEBUG( !iGesture->IsHolding(), Panic( EGesturePanicIllegalLogic ) );
                    if ( !iPreviousTapGesture )
                        {
                        // First tap. Delay emitting its code evemt and released events until it is known
                        // whether the tap is a double tap
                        iPreviousTapGesture = iGesture;
                        iGesture = NewGesture();
                        iDoubleTapTimer->Start(); 
                        }
                    else
                        {
                        // This is a second tap of a double tap. Do not emit anything for the second
                        // tap. Only down event has been emitted for the first tap. Emit the code 
                        // event (double tap) and released for the first tap.
                        iPreviousTapGesture->SetDoubleTap();
                        EmitFirstTapEventL();
                        }
                    }
                else 
                    {
                    // modified iGesture to be "released"
                    CompleteAndEmitSingleTouchL( *iGesture );
                    }
                }
            else
                { // adding a point failed
                EmitCancelEventL();
                }
            // reset state
            CleanupStack::PopAndDestroy( this ); 
            break;
            
        default:
            break;
        }
    return ETrue; // consume
    }

// ----------------------------------------------------------------------------
// Handle multiple pointer events
// ----------------------------------------------------------------------------
//
TBool CGestureHelperImpl::HandleMultiplePointerEventL( const TPointerEvent& aEvent,
       const TInt aPointerNumber )
    { 
    switch ( aEvent.iType )
        {
        case TPointerEvent::EButton1Down:
            // adding the first point implicitly makes the state "not idle"
            AddMultiTouchPointsL( aEvent, aPointerNumber);
            EmitEventL( *iGesture );
            break;
            
        case TPointerEvent::EDrag:
            // While stylus down, the same event is received repeatedly
            // even if stylus does not move. Filter out by checking if point 
            // is the same as the latest point
            if( AddMultiTouchPointsL(aEvent, aPointerNumber ))
                {
                // as long as the starting gesture is seen as a tap, do not emit any
                // drag events
                if ( iGesture->IsPinch() )
                    {
                    // emit the pinch event to client
                    EmitEventL( *iGesture );
                    }
                // else: do not emit any events to the client               
                }

            
            break;

        case TPointerEvent::EButton1Up:
                        
            // if adding of the point fails, notify client with a 
            // cancelled event. It would be wrong to send another
            // gesture code when the up point is not known
            AddMultiTouchPointsL(aEvent, aPointerNumber );
            CompleteAndEmitDoubleTouchL( *iGesture );

            break;
            
        default:
            break;
        }   
    return ETrue; // consume
    }

// ----------------------------------------------------------------------------
// Validate AddMultiTouchPointsL events
// Check if the point recieved is the repeavt event of previous point or a new point 
// inline ok in cpp file for a private member function
// ----------------------------------------------------------------------------
//
inline TBool CGestureHelperImpl::AddMultiTouchPointsL( const TPointerEvent& aEvent, const TInt aPointerNumber)
    {
    TBool pointAdded = EFalse;
    if ( aPointerNumber > 0 )
        {
        if ( !iGesture->IsLatestSecondaryPoint( Position( aEvent ) ) )
            {
            AddSecondaryPointL( aEvent );
            pointAdded = ETrue;
            }                 
        }
    else
        {
        if ( !iGesture->IsLatestPoint( Position( aEvent ) ) )
            {
            AddPointL( aEvent );
            pointAdded = ETrue;
            }                
        }
    return pointAdded;
    }

// ----------------------------------------------------------------------------
// Check for Stray evnet
// ----------------------------------------------------------------------------
//
TBool CGestureHelperImpl::StrayEvent( const TPointerEvent& aEvent ) const
    {
     //If we are recieving a button down on pointer 0 in pointer capturer then its a stray event
    // Dont consume it
    if ( aEvent.iType == TPointerEvent::EButton1Down && IsIdle( 0) )
        {
        return ETrue; // don't consume
        }  
	return EFalse;  
    }
// ----------------------------------------------------------------------------
// Validate the events
// is it a valid event or a stray pointer form some other visuals
// inline ok in cpp file for a private member function
// ----------------------------------------------------------------------------
//
inline TBool CGestureHelperImpl::ValidatePointer( const TPointerEvent& aEvent, const TInt aPointerNumber) const
    {
    // Check if received event is valid or not.
    // In practice, event is NOT valid in the following situations:
    //
    // 1. Pointer down event is received for pointer which is already down.
    // 2. Pointer up event is received for pointer which is already up.
    // 3. Pointer drag event is received for pointer which is not down.
    // 4. Pointer numbers other than 0 and 1. We are handling only 2 pointers.
    //
    // In these situations this function returns EFalse,
    // corresponding event is ignored and recognition continues as earlier.
    //
    
    // filter all the events for which the pointer number is less than 0
    // or greater than 1. we will handle only 2 pointer events.  
    if( aPointerNumber >= 2 || aPointerNumber < 0 )
        {
        return EFalse;
        }
        
    // filter out events that do not start with button down. It is a stray
    // event from another visual    
    TInt pointerTovalidate = aPointerNumber ;
    if( iPointerCount == 1 && aPointerNumber == 1)
	    {
	    pointerTovalidate = 0;
	    }
    if ( aEvent.iType != TPointerEvent::EButton1Down && IsIdle( pointerTovalidate ) )
        {
        return EFalse; // don't consume
        }

    
    return ETrue;
    }

// ----------------------------------------------------------------------------
// Is the helper idle?
// Checks whether any points are already added in the array corresponding
// to the pointer number 
// inline ok in cpp file for a private member function
// ----------------------------------------------------------------------------
//
inline TBool CGestureHelperImpl::IsIdle( TBool aPointerNumber ) const
    {
    if( aPointerNumber == 0 )
        {
        return iGesture->IsEmpty();
        }
    else if( aPointerNumber == 1 )
        {
        return iGesture->IsMultiTouch();
        }
    return ETrue;
    }

// ----------------------------------------------------------------------------
// Add a point to the sequence of points that together make up the gesture
// inline ok in cpp file for a private member function
// ----------------------------------------------------------------------------
//
inline void CGestureHelperImpl::AddPointL( const TPointerEvent& aEvent )
    {
    User::LeaveIfError( AddPoint( aEvent ) );
    }

// ----------------------------------------------------------------------------
// Add a point to the sequence of points that together make up the gesture
// inline ok in cpp file for a private member function
// ----------------------------------------------------------------------------
//
inline TInt CGestureHelperImpl::AddPoint( const TPointerEvent& aEvent )
    {
    return iGesture->AddPoint( Position ( aEvent ) );
    }

// ----------------------------------------------------------------------------
// Add a point to the sequence of points that together make up the gesture
// inline ok in cpp file for a private member function
// ----------------------------------------------------------------------------
//
inline void CGestureHelperImpl::AddSecondaryPointL( const TPointerEvent& aEvent )
    {
    User::LeaveIfError( AddSecondaryPoint( aEvent ) );
    }

// ----------------------------------------------------------------------------
// Add a point to the sequence of points that together make up the gesture
// inline ok in cpp file for a private member function
// ----------------------------------------------------------------------------
//
inline TInt CGestureHelperImpl::AddSecondaryPoint( const TPointerEvent& aEvent )
    {
    return iGesture->AddSecondaryPoint( Position ( aEvent ) );
    }

// ----------------------------------------------------------------------------
// StartHoldingTimer
// ----------------------------------------------------------------------------
//
void CGestureHelperImpl::StartHoldingTimer( const TPointerEvent& aNewEvent )
    {
    if ( !( iGesture->IsHolding() ||
            iGesture->IsNearHoldingPoint( Position( aNewEvent ) ) ) )
        {
        // restart hold timer, since pointer has moved
        iHoldingTimer->Start();
        // Remember the point in which holding was started
        iGesture->SetHoldingPoint();
        }
    }

/** 
 * Helper function that calls ContinueHolding on the pointer to TGesture
 */
static void ContinueHolding( TAny* aGesture )
    {
    static_cast< CGesture* >( aGesture )->ContinueHolding();
    }

// ----------------------------------------------------------------------------
// Add a point to the sequence of points that together make up the gesture
// ----------------------------------------------------------------------------
//
void CGestureHelperImpl::StartHoldingL()
    {
    // hold & tap event is specifically filtered out. Use case: in list fast 
    // scrolling activation (e.g. enhanced coverflow), tap & hold should not
    // start fast scroll. In addition, after long tap on start position,
    // drag and drag & hold swiping should emit normal swipe and swipe&hold
    // events. Therefore, tap & hold is not supported.
    __ASSERT_DEBUG( !iGesture->IsTap() && !iPreviousTapGesture, Panic( EGesturePanicIllegalLogic ) );
    
    // holding has just started, and gesture code should be provided to client.
    // set gesture state so that it produces a gesture code (other than drag)
    iGesture->StartHolding();
    
    // create an item in the cleanup stack that will set the gesture state
    // to holding-was-started-earlier state. NotifyL may leave, but the
    // holding-was-started-earlier state must still be successfully set,
    // otherwise, the holding gesture code will be sent twice
    CleanupStack::PushL( TCleanupItem( &ContinueHolding, iGesture ) );
    
    EmitEventL( *iGesture );
    
    // set holding state to "post holding"
    CleanupStack::PopAndDestroy( iGesture );
    }

// ----------------------------------------------------------------------------
// RecyclePreviousTapGesture
// ----------------------------------------------------------------------------
//
void CGestureHelperImpl::RecyclePreviousTapGesture( TAny* aSelf )
    {
    CGestureHelperImpl& self = *reinterpret_cast<CGestureHelperImpl*>( aSelf );
    self.RecycleGesture( self.iPreviousTapGesture );
    }

// ----------------------------------------------------------------------------
// Emit the remainder of the previous tap event (tap + released)
// ----------------------------------------------------------------------------
//
void CGestureHelperImpl::EmitFirstTapEventL()
    {
    // when this function is called, a tap has turned out to _not_ be a double tap
    __ASSERT_DEBUG( IsDoubleTapEnabled(), Panic( EGesturePanicIllegalLogic ) );
    __ASSERT_DEBUG( iPreviousTapGesture, Panic( EGesturePanicIllegalLogic ) );
    
    iDoubleTapTimer->Cancel();
    
    // ensure previous tap gesture is reset even if client leaves
    CleanupStack::PushL( TCleanupItem( &RecyclePreviousTapGesture, this ) );
    
    CompleteAndEmitSingleTouchL( *iPreviousTapGesture );
    
    // recycle the emitted gesture 
    CleanupStack::PopAndDestroy( this ); 
    }

// ----------------------------------------------------------------------------
// EmitStartEventL
// ----------------------------------------------------------------------------
//
void CGestureHelperImpl::EmitStartEventL( const CGesture& aGesture )    
    {
    CGesture* startGesture = aGesture.AsStartEventLC();
    EmitEventL( *startGesture );
    CleanupStack::PopAndDestroy( startGesture );    
    }
    
// ----------------------------------------------------------------------------
// EmitCompletionEvents for single touch
// ----------------------------------------------------------------------------
//
void CGestureHelperImpl::CompleteAndEmitSingleTouchL( CGesture& aGesture )
    {
    aGesture.SetSingleTouchComplete();
    // send gesture code if holding has not been started. If holding has 
    // been started, client has already received a "hold swipe left" e.g. event, in which
    // case don't another "swipe left" event
    if ( !aGesture.IsHolding() )
        {
        // if client leaves, the state is automatically reset.
        // In this case the client will not get the released event
        EmitEventL( aGesture ); 
        }
    // send an event that stylus was lifted
    aGesture.SetSingleTouchReleased();
    EmitEventL( aGesture ); 
    }
 
// ----------------------------------------------------------------------------
// EmitCompletionEvents for double touch
// ----------------------------------------------------------------------------
//
void CGestureHelperImpl::CompleteAndEmitDoubleTouchL( CGesture& aGesture )
    {
    aGesture.SetMultiTouchComplete();
    if ( aGesture.IsPinch() )
        {
        // emit the pinch event to client
        EmitEventL( aGesture );
        }
    // send an event that stylus was lifted
    aGesture.SetMultiTouchReleased();
    EmitEventL( aGesture ); 
    }

// ----------------------------------------------------------------------------
// EmitCancelEventL
// ----------------------------------------------------------------------------
//
void CGestureHelperImpl::EmitCancelEventL()
    {
    iDoubleTapTimer->Cancel();

    // ensure previous tap gesture is reset even if client leaves
    CleanupStack::PushL( TCleanupItem( &RecyclePreviousTapGesture, this ) );

    CGesture& gestureToCancel = iPreviousTapGesture ? *iPreviousTapGesture : *iGesture;
    gestureToCancel.SetCancelled();
    EmitEventL( gestureToCancel );
    
    // recycle the emitted gesture 
    CleanupStack::PopAndDestroy( this ); 
    }

// ----------------------------------------------------------------------------
// Notify observer
// ----------------------------------------------------------------------------
//
void CGestureHelperImpl::EmitEventL( const CGesture& aGesture )
    {
    // iPoints array must have content when this function is called
    iObserver->HandleGestureL( aGesture );
    }

// ----------------------------------------------------------------------------
// Return a fresh gesture from the gesture pool (pool of one gesture)
// ----------------------------------------------------------------------------
//
CGesture* CGestureHelperImpl::NewGesture()
    {
    __ASSERT_DEBUG( iUnusedGesture, Panic( EGesturePanicIllegalLogic ) ); // pool should no be empty
    iUnusedGesture->Reset();
    CGesture* freshGesture = iUnusedGesture;
    iUnusedGesture = NULL;
    return freshGesture;
    }

// ----------------------------------------------------------------------------
// Return a fresh gesture from the gesture pool (pool of one gesture)
// ----------------------------------------------------------------------------
//
void CGestureHelperImpl::RecycleGesture( CGesture*& aGesturePointer )
    {
    // only one object fits into the pool, and that should currently be enough
    // one pointer must be null, one non-null
    __ASSERT_DEBUG( !iUnusedGesture != !aGesturePointer, Panic( EGesturePanicIllegalLogic ) );
    if ( aGesturePointer )
        {
        iUnusedGesture = aGesturePointer;
        aGesturePointer = NULL;
        }
    }

// ----------------------------------------------------------------------------
// AddObserver
// ----------------------------------------------------------------------------
//
void CGestureHelperImpl::AddObserver(MGestureObserver* aObserver)
    {
    iObserver = aObserver;
    }
	
// end of file
	
