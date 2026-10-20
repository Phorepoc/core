/*
* Copyright (c) 2008 Nokia Corporation and/or its subsidiary(-ies).
* All rights reserved.
* This component and the accompanying materials are made available
* under the terms of the License "Eclipse Public License v1.0"
* which accompanies this distribution, and is available
* at the URL "http://www.eclipse.org/legal/epl-v10.html".
*
* Initial Contributors:
* Nokia Corporation - initial contribution.
*
* Contributors:
*
* Description:   
*
*/

#include <coemain.h>
#include <aknutils.h>
#include <aknappui.h>

#include "statemachine.h"
#include "stateengine.h"
#include "rt_uievent.h"
#include "callbacktimer.h"
#include "stateengineconfiguration.h"

#include "filelogger.h"
#include "utils.h"

using namespace stmUiEventEngine ;

GLREF_D const char* stateNames[8] ;

#ifdef GESTURE_LOGGING
const char* const ttypeNames[] = {  // for debugging purposes define the names of the pointer events
            "EButton1Down         ",
            "EButton1Up           ",
            "EButton2Down         ",
            "EButton2Up           ",
            "EButton3Down         ",
            "EButton3Up           ",
            "EDrag                ",
            "EMove                ",
            "EButtonRepeat        ",
            "ESwitchOn            ",
            "EOutOfRange          ",
            "EEnterCloseProximity ",
            "EExitCloseProximity  ",
            "EEnterHighPressure   ",
            "EExitHighPressure    "
            };
#endif

/// Fast integer distance
int stmUiEventEngine::Distance(int x, int y)
{
/*
    double d = dx * dx + dy * dy;
    double dist ;
    Math::Sqrt(dist, d) ;
    return dist;
*/
    if(x<0) x=-x;
    if(y<0) y=-y;
    if(x < y)
    {
      int t = x;
      x = y;
      y = t;        // ensures that x >= y
    }
    int dist = (y < ((13107 * x)>>15)) ?    // * (.4)
              (x + ((y * 6310)>>15)) :      // * (.192582403)
              (((x * 27926)>>15)            // * (.852245894)
                 + ((y * 18414)>>15));      // * (.561967668)
    return dist;
}


/*!
  CStateMachine implements the state machine and the integration fo the
  state machine to the OS.  The CStateEngine handles the actual processing of the
  finite state machine but CStateMachine provides the OS specific things like timers
  and message conversion.

  The CStateMachine implements the MAknWsEventObserver interface so it adds
  itself to be the observer to the event monitor of the application UI.

  It handles the pointer events either by using the monitoring interface or
  lets the application call the HandlePointerEventL method.

  If MAknWsEventObserver interface is used then all the events passed to the
  application are seen.  The target of the gesture starting event is stored
  so that the generated UI event contains the target as a void pointer. (should it just be CoeControl*?)

  There is possibility to adjust the Y coordinate of the touch point.
  In capacitive touch (Alvin) it seems that the perceived touch point is
  below the middle part of the fingertip.  The user however tries to use the
  finger to point so that the touch point should correspond to the tip of the finger.
  It seems that this illusion can be achieved by adjusting the Y position about 3 mm up.
  However, this adjustment can properly be done only when the touch point is far enough
  from the window borders up or down.  When close to top or bottom of window, the adjustment
  makes it impossible touch points near the edge, unless adjustment depends on the distance from
  window border.
  So in practice it should be the window server doing the adjustment, and after adjustment
  deciding the target window.  At application level the adjustment can only be done properly
  if window borders do not need to be crossed.

 */
CStateMachine::CStateMachine()
{
    m_WasMessageFiltered = false ;
    m_wseventmonitoringenabled = false ; // NB: enabled only if really used by application
    m_loggingenabled = false ;
    m_capacitiveup = false ;
    m_adjustYposition = false ;
}
/*!Destructor
 */
CStateMachine::~CStateMachine()
{
    for (int i = 0; i < KMaxNumberOfPointers; i++)
    {
        delete m_holdTimer[i] ;
        delete m_touchTimer[i] ;
        delete m_suppressTimer[i] ;
        delete m_impl[i] ;
    }
    delete m_config ;
}

CStateMachine* CStateMachine::NewLC()
{
    CStateMachine* self = new (ELeave) CStateMachine();
    CleanupStack::PushL(self);
    self->ConstructL();
    return self;
}

CStateMachine* CStateMachine::NewL()
{
    CStateMachine* self = CStateMachine::NewLC();
    CleanupStack::Pop(self); 
    return self;
}
/*!
 Construct the actual state machine implemented in CStateEngine and
 creates the timers.  It also adds itself to the observer list of
 CAknWsEventMonitor of the application.

 */
void CStateMachine::ConstructL()
{
    m_config = new(ELeave)CStateEngineConfiguration() ;
    m_config->ConstructL();

    for (int i = 0; i < KMaxNumberOfPointers; i++)
    {
        m_impl[i] = new(ELeave) CStateEngine(m_config, this, i) ;
        m_holdTimer[i] = CCallbackTimer::NewL(*this, &CStateMachine::handleholdTimer, 0, i, ETrue);
        m_touchTimer[i] = CCallbackTimer::NewL(*this, &CStateMachine::handletouchTimer, 0, i, ETrue);
        m_suppressTimer[i] = CCallbackTimer::NewL(*this, &CStateMachine::handlesuppressTimer, 0, i, ETrue);
    }

    m_coeEnv = CCoeEnv::Static();

    /* IMEX: monitor added only if enabled
      add us to see the WsEvents so that we are able to interpret them;

    CAknAppUi* pui = (CAknAppUi*)m_coeEnv->AppUi() ;
    pui->EventMonitor()->AddObserverL(this) ;
    pui->EventMonitor()->Enable(ETrue) ;
    */

    m_3mminpixels = Mm2Pixels(1.5) ;
}
/*!
 * Process one pointer event in the state machine.
 * \return true, if the event did not generate a UI event immediately.
 */
bool CStateMachine::HandleStateEvent(const TPointerEvent& aPointerEvent, 
                                     void* aTarget, 
                                     const TTime& aTime)
{
    TInt index = PointerIndex(aPointerEvent);
    CStateEngine* engine = m_impl[index];
    CreateHwEvent(engine->initEvent(), aPointerEvent, aTarget, aTime) ;
    if (m_loggingenabled)
    {
        LOGARG("HandleStateEvent: ptr %d", index) ;
    }
    m_WasMessageFiltered = engine->handleStateEvent() ;
    return m_WasMessageFiltered ;
}
/*!
 * wrapper method for the actual implementation in the CStateEngine.
 */
bool CStateMachine::wasLastMessageFiltered(TInt aPointerNumber)
{
    return m_impl[aPointerNumber]->wasLastMessageFiltered() ;
}
/*!
 * wrapper method for the actual implementation in the CStateEngine.
 */
TRect CStateMachine::getTouchArea(TInt aPointerNumber)
{
    return m_impl[aPointerNumber]->getTouchArea() ;
}
/*!
 * wrapper method for the actual implementation in the CStateEngine.
 */
void CStateMachine::setTouchTimeArea(long fingersize_mm)
{
    m_config->setTouchTimeArea(fingersize_mm) ;
}
/*!
 * wrapper method for the actual implementation in the CStateEngine.
 */
void CStateMachine::setTouchArea(long fingersize_mm)
{
    m_config->setTouchArea(fingersize_mm) ;
}
/*!
 * wrapper method for the actual implementation in the CStateEngine.
 */
TAreaShape CStateMachine::getTouchAreaShape()
{
    return m_config->getTouchAreaShape() ;
}
/*!
 * wrapper method for the actual implementation in the CStateEngine.
 */
void CStateMachine::setTouchAreaShape(const TAreaShape aShape)
{
    m_config->setTouchAreaShape(aShape) ;
}
/*!
 * wrapper method for the actual implementation in the CStateEngine.
 */
unsigned int CStateMachine::getTouchTimeout()
{
    return m_config->getTouchTimeout() ;
}
/*!
 * wrapper method for the actual implementation in the CStateEngine.
 */
void CStateMachine::setTouchTimeout(unsigned int aDelay)
{
    m_config->setTouchTimeout(aDelay) ;
}
/*!
 * wrapper method for the actual implementation in the CStateEngine.
 */
TRect CStateMachine::getHoldArea(TInt aPointerNumber)
{
    return m_impl[aPointerNumber]->getHoldArea() ;
}
/*!
 * wrapper method for the actual implementation in the CStateEngine.
 */
void CStateMachine::setHoldArea(long fingersize_mm)
{
    m_config->setHoldArea(fingersize_mm) ;
}
/*!
 * wrapper method for the actual implementation in the CStateEngine.
 */
TAreaShape CStateMachine::getHoldAreaShape()
{
    return m_config->getHoldAreaShape() ;
}
/*!
 * wrapper method for the actual implementation in the CStateEngine.
 */
void CStateMachine::setHoldAreaShape(const TAreaShape aShape)
{
    m_config->setHoldAreaShape(aShape) ;
}
/*!
 * wrapper method for the actual implementation in the CStateEngine.
 */
unsigned int CStateMachine::getHoldTimeout()
{
    return m_config->getHoldTimeout() ;
}
/*!
 * wrapper method for the actual implementation in the CStateEngine.
 */
void CStateMachine::setHoldTimeout(unsigned int a)
{
    m_config->setHoldTimeout(a) ;
}
/*!
 * wrapper method for the actual implementation in the CStateEngine.
 */
unsigned int CStateMachine::getTouchSuppressTimeout()
{
    return m_config->getTouchSuppressTimeout() ;
}
/*!
 * wrapper method for the actual implementation in the CStateEngine.
 */
void CStateMachine::setTouchSuppressTimeout(unsigned int a)
{
    m_config->setTouchSuppressTimeout(a) ;
}
/*!
 * wrapper method for the actual implementation in the CStateEngine.
 */
unsigned int CStateMachine::getMoveSuppressTimeout()
{
    return m_config->getMoveSuppressTimeout() ;
}
/*!
 * wrapper method for the actual implementation in the CStateEngine.
 */
void CStateMachine::setMoveSuppressTimeout(unsigned int a)
{
    m_config->setMoveSuppressTimeout(a) ;
}
/*!
 * wrapper method for the actual implementation in the CStateEngine.
 */
bool CStateMachine::addUiEventObserver(MUiEventObserver* observer)
{
    return m_config->addUiEventObserver(observer) ;
}
/*!
 * wrapper method for the actual implementation in the CStateEngine.
 */
bool CStateMachine::removeUiEventObserver(MUiEventObserver* observer)
{
    return m_config->removeUiEventObserver(observer) ;
}
/*!
 * wrapper method for the actual implementation in the CStateEngine.
 */
void CStateMachine::enableLogging(bool aEnable)
{
    m_loggingenabled = aEnable ;
    m_config->enableLogging(aEnable) ;
}

TInt CStateMachine::PointerIndex(const TPointerEvent& aPointerEvent)
{
    TInt index = 0;
#if defined(ADVANCED_POINTER_EVENTS)
    if (aPointerEvent.IsAdvancedPointerEvent())
    {
        const TAdvancedPointerEvent* tadvp = aPointerEvent.AdvancedPointerEvent() ;
        index = tadvp->PointerNumber() ;
    }
#endif
    return index;
}

/*!
 * Convert pointer event (TPointerEvent) into THwEvent into the variable m_hwe.
 * THwEvent contains the position and simplified event type
 * but also has the target window and timestamp included.
 */
void CStateMachine::CreateHwEvent(THwEvent& aEvent, 
                                  const TPointerEvent& aPointerEvent, 
                                  void* aTarget, 
                                  const TTime& aTime)
{
/*  should be set by this moment by CStateEngine with that index in CStateEngine::initEvent()
    aEvent.iPointerNumber = PointerIndex(aPointerEvent);
*/
    aEvent.iTarget = aTarget ;
    aEvent.iTime = aTime;
    // Change to screen coordinates here while the window is still existing....
    aEvent.iPosition = screenCoordinates(aPointerEvent.iPosition, aTarget) ;
    switch (aPointerEvent.iType)
    {
    case TPointerEvent::EButton1Down:
    case TPointerEvent::EButton2Down:
    case TPointerEvent::EButton3Down:
    {
        aEvent.iType = stmUiEventEngine::EDown ;
        break ;
    }
    case TPointerEvent::EButton1Up:
    case TPointerEvent::EButton2Up:
    case TPointerEvent::EButton3Up:
    {
        if (m_capacitiveup)
        {
            aEvent.iType = stmUiEventEngine::ECapacitiveUP ;   // How could this be checked automagically?
        }
        else
        {
            aEvent.iType = stmUiEventEngine::EResistiveUP ;    // How could this be checked automagically?
        }

        break ;
    }
    case TPointerEvent::EDrag:
    {
        aEvent.iType = stmUiEventEngine::EDrag ;
        break ;
    }
    }
}
////////////////////////////////////////////////////////////////////////////////////////////
/*!
 * Start the hold timer if it is not already active.
 */
void CStateMachine::startholdTimer(TInt aPointerNumber)
{
    if (!m_holdTimer[aPointerNumber]->IsActive())
    {
        m_holdTimer[aPointerNumber]->Start();
    }
}

/*!
 * The hold timer expiration: create a timer event and call the state machine
 */
void CStateMachine::handleholdTimer(TInt aPointerNumber)
{
    // We get an event, lets kick the state machine
    CStateEngine* engine = m_impl[aPointerNumber];
    CreateTimerEvent(engine->initEvent(), stmUiEventEngine::EHoldTimer) ;
    engine->handleStateEvent() ;
}
/*!
 * Stop the hold timer
 */
void CStateMachine::cancelholdTimer(TInt aPointerNumber)
{
    m_holdTimer[aPointerNumber]->Cancel();
}
/*!
 * Start suppress timer.  The timeout has been set beforehand.
 */
void CStateMachine::startsuppressTimer(TInt aPointerNumber)
{
    m_suppressTimer[aPointerNumber]->Start();
}
/*!
 * The suppress timer expiration, create a timer event and call the state machine.
 */
void CStateMachine::handlesuppressTimer(TInt aPointerNumber)
{
    // We get an event, lets kick the state machine
    CStateEngine* engine = m_impl[aPointerNumber];
    CreateTimerEvent(engine->initEvent(), stmUiEventEngine::ESuppressTimer) ;
    engine->handleStateEvent() ;
}
/*!
 * stop the suppress timer
 */
void CStateMachine::cancelsuppressTimer(TInt aPointerNumber)
{
    m_suppressTimer[aPointerNumber]->Cancel();
}
/*!
 * start the touch timer if it is not already active.
 */
void CStateMachine::starttouchTimer(TInt aPointerNumber)
{
    if (!m_touchTimer[aPointerNumber]->IsActive())
    {
        m_touchTimer[aPointerNumber]->Start();
    }
}

/*!
 * The touch timer expiration, create timer event and call the state machine.
 */
void CStateMachine::handletouchTimer(TInt aPointerNumber)
{
    // We get an event, lets kick the state machine
    CStateEngine* engine = m_impl[aPointerNumber];
    CreateTimerEvent(engine->initEvent(), stmUiEventEngine::ETouchTimer) ;
    engine->handleStateEvent() ;
}
/*!
 * stop the touch timer
 */
void CStateMachine::canceltouchTimer(TInt aPointerNumber)
{
    if (m_touchTimer[aPointerNumber]->IsActive()) // we were waiting for additional events
    {
        m_touchTimer[aPointerNumber]->Cancel();
    }
}
/*!
 * CreateTimerEvent creates a timer event to the m_hwe variable.
 */
void CStateMachine::CreateTimerEvent(THwEvent& aEvent, TStateMachineEvent aEventCode)
{
    aEvent.iType = aEventCode ;
    // m_hwe.iPosition = TPos(0, 0) ;      should we just leave the previous pos
    TTime now ;
    now.HomeTime() ;
    aEvent.iTime = now ;
}
/*!
 * Events are processed either using the MAknWsEventObserver interface
 * or letting the view/container pass the pointer events to the state machine.
 * The member variable m_wseventmonitoringenabled defines which method is used.
 * If handlePoingterEventL is called, the calling CCoeCOntrol must provide itself
 * as the target.
 */
void CStateMachine::HandlePointerEventL(const TPointerEvent& aPointerEvent, void *target)
{
    if (m_wseventmonitoringenabled) return ;    // events are handled in the event monitor
    if (m_loggingenabled)
    {
#if defined(ADVANCED_POINTER_EVENTS)
        TInt pointerNumber = PointerIndex(aPointerEvent) ;
        LOGARG("Pointer %d event %s at (%d %d)", pointerNumber,
                ttypeNames[aPointerEvent.iType], aPointerEvent.iPosition.iX, aPointerEvent.iPosition.iY) ;
#else
        LOGARG("Pointer event %s at (%d %d)",
                ttypeNames[aPointerEvent.iType], aPointerEvent.iPosition.iX, aPointerEvent.iPosition.iY) ;
#endif
    }
    TTime time = m_coeEnv->LastEvent().Time();
    HandleStateEvent(aPointerEvent, target, time) ;   // target needs to be there to convert from window to screen coordinates
}
/**
 * One possibility to implement gesture recognition is to intercept the events
 * using the event monitoring interface.  The HandleWsEventL method will get all events
 * passed to the application.  The aDestination parameter defines the window where the event
 * was targeted to.  The gesture recognition should use the target information
 * to determine how the gesture should be interpreted.
 * In the current implementation the aDestination needs to be one of the UI event observers
 * in order to process the message, but later when gesture recognition is added, this check
 * needs to be removed and the gesture recognition should handle the target of the gesture.
 */
void CStateMachine::HandleWsEventL(const TWsEvent& aEvent, CCoeControl* aDestination)
{
    // Check which processing type we have for events.
    // If WsEvent monitoring, then process the message, otherwise return
    if (!m_wseventmonitoringenabled) return ;

    // Log the events passing trough to see what kind of stuff there goes...
    // and could the gesture recogniser grab them from here?
    TInt type=aEvent.Type();
    switch (type)
        {
    case EEventKey:
    case EEventKeyUp:
    case EEventKeyDown:
    {
        if (m_loggingenabled)
        {
            TKeyEvent* tke = aEvent.Key() ;
            LOGARG("Key event %d %d to %u", tke->iCode, tke->iScanCode, aDestination) ;
        }
        break;
    }
    case EEventPointer:
    {
        TPointerEvent* tpe = aEvent.Pointer() ;
        if (m_loggingenabled)
        {
            TRect rcd = aDestination->Rect() ;
            TPoint org = aDestination->PositionRelativeToScreen() ;
            TRect rcd2 = rcd ;
            rcd2.Move(org) ;
            TPoint screenpos = tpe->iPosition ;
            screenpos += org ;

#if defined(ADVANCED_POINTER_EVENTS)
            TInt pointerNumber = PointerIndex(*tpe) ;
            LOGARG("Pointer %d event %s at (%d %d)[%d,%d] to 0x%x ((%d,%d)(%d,%d)): screen: ((%d,%d)(%d,%d))",
                    pointerNumber,
                    ttypeNames[tpe->iType],
                    tpe->iPosition.iX, tpe->iPosition.iY,
                    screenpos.iX, screenpos.iY,
                    aDestination,
                rcd.iTl.iX, rcd.iTl.iY, rcd.iBr.iX, rcd.iBr.iY,
                rcd2.iTl.iX, rcd2.iTl.iY, rcd2.iBr.iX, rcd2.iBr.iY) ;
#else
            LOGARG("Pointer event %s at (%d %d)[%d,%d] to 0x%x ((%d,%d)(%d,%d)): screen: ((%d,%d)(%d,%d))",
                    ttypeNames[tpe->iType], tpe->iPosition.iX, tpe->iPosition.iY,
                    screenpos.iX, screenpos.iY,
                    aDestination,
                rcd.iTl.iX, rcd.iTl.iY, rcd.iBr.iX, rcd.iBr.iY,
                rcd2.iTl.iX, rcd2.iTl.iY, rcd2.iBr.iX, rcd2.iBr.iY) ;
#endif
        }
        HandleStateEvent(*tpe, aDestination, aEvent.Time()) ;
        break;
    }
    case EEventPointerBufferReady:
        if (m_loggingenabled)
        {
            LOGARG("Pointer buffer ready event to %u", aDestination) ;
        }
        break;
    case EEventFocusLost:
    case EEventFocusGained:
        if (m_loggingenabled)
        {
            LOGARG("Focus message event to %u", aDestination) ;
        }
        break;
    case EEventSwitchOn:
        if (m_loggingenabled)
        {
            LOGARG("Switch On event to %u", aDestination) ;
        }
        break;
    case EEventUser:
        if (m_loggingenabled)
        {
            LOGARG("User event to %u", aDestination) ;
        }
        break;
    case EEventPowerMgmt:
        if (m_loggingenabled)
        {
            LOGARG("Power Mgmnt event to %u", aDestination) ;
        }
        break;
    case EEventMessageReady:
        if (m_loggingenabled)
        {
            LOGARG("Message Ready event to %u", aDestination) ;
        }
        break;
    case EEventScreenDeviceChanged:
        if (m_loggingenabled)
        {
            LOGARG("Screen device changed event to %u", aDestination) ;
        }
        break;

    default:
        if (m_loggingenabled)
        {
            LOGARG("default changed event %d to %u", type, aDestination) ;
        }
        break;
    }
}
/*!
 * Start the touch timer using a specified delay
 */
void CStateMachine::startTouchTimer(TInt aDelay, TInt aPointerNumber)
{
    m_touchTimer[aPointerNumber]->SetDelay(aDelay) ;
    starttouchTimer(aPointerNumber) ;
}
/*!
 *  Stop the touch timer.
 */
void CStateMachine::cancelTouchTimer(TInt aPointerNumber)
{
    canceltouchTimer(aPointerNumber) ;
}
/*!
 * Start hold timer using specified delay
 */
void CStateMachine::startHoldTimer(TInt aDelay, TInt aPointerNumber)
{
    m_holdTimer[aPointerNumber]->SetDelay(aDelay) ;
    startholdTimer(aPointerNumber) ;
}
/*!
 * Stop the hold timer
 */
void CStateMachine::cancelHoldTimer(TInt aPointerNumber)
{
    cancelholdTimer(aPointerNumber) ;
}
/*!
 * Start suppress timer using specified delay.
 */
void CStateMachine::startSuppressTimer(TInt aDelay, TInt aPointerNumber)
{
    m_suppressTimer[aPointerNumber]->SetDelay(aDelay) ;
    startsuppressTimer(aPointerNumber) ;
}
/*!
 *  Stop the suppress timer.
 */
void CStateMachine::cancelSuppressTimer(TInt aPointerNumber)
{
    cancelsuppressTimer(aPointerNumber) ;
}

/*!
 * Method sets the m_wseventmonitoringenabled.  If it is true,
 * then the state machine will be called from the HandleWsEventL method which
 * sees all the events passed to the application.
 *
 * Otherwise the HandlePointerEventL method call will cause the call to
 * state machine so the view/container needs to pass the pointer events to the state machine
 * in its own HandlePointerEventL -method.
 *
 */
void CStateMachine::EnableWsEventMonitoring(bool aEnable)
{
    if( !m_wseventmonitoringenabled && aEnable )
    {
        CAknAppUi* pui = (CAknAppUi*)m_coeEnv->AppUi() ;
        TRAPD(err, pui->EventMonitor()->AddObserverL(this)) ;
        if(!err)
            pui->EventMonitor()->Enable(ETrue) ;
    }
    else if( m_wseventmonitoringenabled && !aEnable )
    {
        CAknAppUi* pui = (CAknAppUi*)m_coeEnv->AppUi() ;
        pui->EventMonitor()->RemoveObserver(this) ;
        // Should not disable since it may be not the only user
        //pui->EventMonitor()->Enable(EFalse) ;
    }

    m_wseventmonitoringenabled = aEnable ;
}
TPoint CStateMachine::screenCoordinates(const TPoint& aPos, void* aGestureTarget)
{
    TPoint newPos = aPos ;
    if (aGestureTarget)
    {
       CCoeControl* pcc = (CCoeControl*) aGestureTarget ;
       TPoint tp(TPoint(0,0));
        if (m_adjustYposition)
        {
            TSize sz = pcc->Size() ;

            // If we are running in capacitive touch device,
            // adjust the point up about 3 mm unless we are
            // near top or bottom of the window

            // Y position in the window
            int wY = newPos.iY - tp.iY;
            int edge = 2*m_3mminpixels;

            if (Rng(0, wY, edge - 1))
            {
                // close to the top we adjust suitably so that immediately at the top adjust is 0
                int adjust = wY / 2 ;
                newPos.iY -= adjust ;
                if (m_loggingenabled)
                {
                    LOGARG("adjustment: nY %d tY %d [3mm: %d  adj: %d]", 
                            newPos.iY, tp.iY, m_3mminpixels, adjust) ;
                }

            }
            //else if (Rng(edge, wY, sz.iHeight - edge))
		else
            {
#ifdef GESTURE_LOGGING
                int from = newPos.iY ;
#endif
                newPos.iY -= m_3mminpixels ;
                if (m_loggingenabled)
                {
                    LOGARG("adjustment: %d to %d [3mm: %d  middle]", 
                            from, newPos.iY, m_3mminpixels) ;
                }

            }
        }
        else
        {
            // if the target does not own a window how can we adjust to the screen?
        }
    }
    return newPos ;
}
void CStateMachine::enableCapacitiveUp(bool aEnable)
{
    m_capacitiveup = aEnable ;
}
void CStateMachine::enableYadjustment(bool aEnable)
{
    m_adjustYposition = aEnable ;
}
int CStateMachine::getNumberOfPointers()
{
    return KMaxNumberOfPointers ;
}

