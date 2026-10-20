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


#include "stateengine.h"
#include "utils.h"
#include "uievent.h"
#include "uieventsender.h"
#include "filelogger.h"
//#include "flogger.h"

using namespace stmUiEventEngine ;

/*!
  State definitions glue together the methods of the stateengine.cpp
  so that it will behave as defined in the state machine specification.
  First define the separate elements for each possible event and then tie them together
  to create one state entry.  The state entries then are put to array
  where the index is at the same time also the state ID.

  STATE_ELEMENT arrays define the methods called when handling a message.
  \sa STATE_ELEMENT.
  Note that the last row of STATE_ELEMENT array must NOT have a ConditionFunction entry
  and it must have a NextState entry != Eignore.  Otherwise the state machine will
  not behave correctly.

 */
/*! Add macro with token pasting to make creation of the state machine tables easier
   and removing the necessity to write the classname twice.
   Maybe there would be some better way to do this using templates and typedefs?
 */
#define CND(x) isF<CStateEngine,&CStateEngine::x>
#define ACT(x) aF<CStateEngine,&CStateEngine::x>

/*********************************************************************************
 * empty statedef as a dummy entry
 *  */
const STATE_ELEMENT __ErrorEvent[1] = {
        0,
        ACT(ErrorEvent),
        EInit
};

const STATE Ignore__[1] = {
        EDown,          __ErrorEvent
} ;

/*!
  :INIT state and its event specific elements
  See the spec in http://wikis.in.nokia.com/Runtimes/NewGestureLibrary
  Down is only valid event in :INIT state
  The event is consumed immediately, so that the state machine will process only these
  methods when processing the message.
  If touch timer has been set, the next state is InTouchTime.
  If no touch timer, but if hold timer has been defined, the next state is InHoldTime_U
  If no touch or hold timer have been defined, but touch area has been defined, next state is InTouchArea.
  11-May-2009: addition: add another touch area: one for touch time and one for touch area after touch time
  has elapsed.  This allows "sloppy" touch to be handled properly without extra move if touchtimearea is larger,
  but after touch has been detected a smaller movement is allowed.
 */
const STATE_ELEMENT Init__Down[12] = {
        0,                  ACT(ConsumeEvent),    Eignore,
        0,                  ACT(SetGestureStart), Eignore,
        0,                  ACT(SetCurrentPos),   Eignore,
        CND(IsTouchTimer),  ACT(InitTouchTimer),  Eignore,
        CND(IsHoldTimer),   ACT(InitHoldTimer),   Eignore,
        CND(IsTouchTimeArea),   ACT(PrepareTouchTimeArea),Eignore,
        CND(IsHoldArea),    ACT(PrepareHoldArea) ,Eignore,
        CND(IsTouchTimer),  0,                    EInTouchTime,
        0,                  ACT(ProduceTouch),    Eignore,
        CND(IsHoldTimer),   0,                    EInHoldTime_U,
        CND(IsTouchArea),   ACT(PrepareTouchArea),EInTouchArea,
        0,                  0,                    EDispatch             // If nothing else happens, goto to Dispatch state
};
/**
 * All the rest of the events are errors so produce error entry to log and
 * stay in the Init state
 */
const STATE_ELEMENT Init__ErrorEvent[2] = {
        0,              ACT(ConsumeEvent),    Eignore,  // remember to consume event, otherwise state machine will loop...
        0,              ACT(ErrorEvent),      EInit
};

/*!
 * :INIT
 * note that only valid event is DOWN, all the rest can be handled as errors
 */
const STATE Init__[7] = {
        EDown,          Init__Down,
        EDrag,          Init__ErrorEvent,
        ECapacitiveUP,  Init__ErrorEvent,
        EResistiveUP,   Init__ErrorEvent,
        ETouchTimer,    Init__ErrorEvent,
        EHoldTimer,     Init__ErrorEvent,
        ESuppressTimer, Init__ErrorEvent
};

/*!
 *  :Dispatch state end its elements
 *  Here the valid events are DRAG and the UP events.
 */
const STATE_ELEMENT Dispatch__Drag[7] = {
        0,                  ACT(StoreMovePos),    Eignore,
        0,                  ACT(ConsumeEvent),    Eignore,
        0,                  ACT(SetCurrentPos),   Eignore,
        0,                  ACT(AddDraggingPos),  Eignore,
        0,                  ACT(ProduceMove),     Eignore,
        CND(LooksLikeHold), ACT(InitHoldTimer),EInHoldTime_U,
        0,                  0,                    EDispatch
} ;

const STATE_ELEMENT Dispatch__CapacitiveUp[3] = {
        0,                  ACT(ConsumeEvent),    Eignore,
        0,                  ACT(SetCurrentPos),   Eignore,
        0,                  ACT(ProduceRelease),  EInit
} ;

const STATE_ELEMENT Dispatch__ResistiveUp[4] = {
        0,                   ACT(ConsumeEvent),       Eignore,
        0,                   ACT(SetCurrentPos),   Eignore,
        CND(IsSuppressTimer),ACT(InitMoveSuppressTimer),  ESuppress_D,
        0,                   ACT(ProduceRelease),     EInit
} ;

/*!
 * All the rest of the events are errors so produce error entry to log and
 * stay in the Dispatch state
 * (TODO: note that in the future we may further
 * define the error cases so that they may change state; )
 */
const STATE_ELEMENT DispatchErrorEvent[2] = {
        0,              ACT(ConsumeEvent),    Eignore,  // remember to consume event, otherwise state machine will loop...
        0,              ACT(ErrorEvent),      EDispatch
};

const STATE Dispatch__[7] = {
        EDown,          DispatchErrorEvent,
        EDrag,          Dispatch__Drag,
        ECapacitiveUP,  Dispatch__CapacitiveUp,
        EResistiveUP,   Dispatch__ResistiveUp,
        ETouchTimer,    DispatchErrorEvent,
        EHoldTimer,     DispatchErrorEvent,
        ESuppressTimer, DispatchErrorEvent
};

/*!
 *  :InTouchTime state end its elements
 *  Here the valid events are DRAG and the UP events and the TouchTimer
 */
const STATE_ELEMENT InTouchTime__Drag[6] = {
        0,                  ACT(StoreMovePos),    Eignore,
        CND(InsideTouchTimeArea), ACT(ConsumeEvent),      Eignore,
        CND(InsideTouchTimeArea), ACT(AddToTouch),        EInTouchTime,     // Calculate touch XY as average of the touches
        0,                    ACT(ClearTouchTimer),   Eignore,          // These lines are done only if InsideTouchArea returns false
        0,                    ACT(ClearHoldTimer),    Eignore,
        0,                    ACT(ProduceTouch),      EDispatch

} ;
/**
 * Note that ConsumeEvent is missing so after doing this the state engine will do EDispatch
 */
const STATE_ELEMENT InTouchTime__CapacitiveUp[4] = {
        0,                  ACT(SetCurrentPos),       Eignore,
        0,                  ACT(ClearTouchTimer),     Eignore,
        0,                  ACT(ClearHoldTimer),      Eignore,
        0,                  ACT(ProduceTouch),        EDispatch
} ;
/**
 * Note that ConsumeEvent is not called if IsHoldTimer returns false, so the Dispatch will be done
 * by the state machine.
 */
const STATE_ELEMENT InTouchTime__ResistiveUp[5] = {
        0,                   ACT(SetCurrentPos),   Eignore,
        0,                   ACT(ClearTouchTimer),    Eignore,
        0,                   ACT(ProduceTouch),       Eignore,
        CND(IsHoldTimer),    0, /*ACT(ConsumeEvent),*/EInHoldTime_U,    // Note that otherwise immediate UP is handled improperly
        0,                   0,                       EDispatch
} ;

const STATE_ELEMENT InTouchTime__TouchTimer[6] = {
        0,                   ACT(ConsumeEvent),       Eignore,
        0,                   ACT(ClearTouchTimer),    Eignore,
        0,                   ACT(ProduceTouch),       Eignore,
        CND(IsTouchArea),    ACT(PrepareTouchArea),Eignore,         // prepare the other touch area
        CND(IsHoldTimer),    0,                       EInHoldTime_U,
        0,                   0,                     EInTouchArea
} ;


/**
 * All the rest of the events are errors so produce error entry to log and
 * stay in the InTouchTime state
 */
const STATE_ELEMENT InTouchTimeErrorEvent[2] = {
        0,              ACT(ConsumeEvent),    Eignore,  // remember to consume event, otherwise state machine will loop...
        0,              ACT(ErrorEvent),      EInTouchTime
};

const STATE InTouchTime__[7] = {
        EDown,          InTouchTimeErrorEvent,
        EDrag,          InTouchTime__Drag,
        ECapacitiveUP,  InTouchTime__CapacitiveUp,
        EResistiveUP,   InTouchTime__ResistiveUp,
        ETouchTimer,    InTouchTime__TouchTimer,
        EHoldTimer,     InTouchTimeErrorEvent,
        ESuppressTimer, InTouchTimeErrorEvent
};

/*!
 *  :InHoldTime_U state end its elements
 *  Here only touch timer event is invalid
 */

const STATE_ELEMENT InHoldTime_U__Down[1] = {
        0,                    0,      EInHoldTime_D  // Note that ConsumeEvent is not called
} ;

const STATE_ELEMENT InHoldTime_U__Drag[3] = {
        0,                  ACT(StoreMovePos),    Eignore,
        CND(InsideHoldArea), ACT(ConsumeEvent),       EInHoldTime_U,
        0,                    ACT(ClearHoldTimer),    EDispatch     // Note that in this case ConsumeEvent is not called
} ;
/**
 * Note that ConsumeEvent is missing so after doing this the state engine will do EDispatch
 */
const STATE_ELEMENT InHoldTime_U__CapacitiveUp[1] = {
        0,                  ACT(ClearHoldTimer),      EDispatch     // Note that ConsumeEvent not called
} ;
/**
 *
 */
const STATE_ELEMENT InHoldTime_U__ResistiveUp[5] = {
        0,                   ACT(ConsumeEvent),  Eignore,
        0,                   ACT(SetCurrentPos),   Eignore,
        CND(IsSuppressTimer),ACT(InitTouchSuppressTimer),  EInHoldTime_D,   // If suppression, start timer and wait for down or timer
        0,                   ACT(ClearHoldTimer), Eignore,  // remember to do this
        0,                   ACT(ProduceRelease), EInit     // No suppression, then this is immediate release
} ;

const STATE_ELEMENT InHoldTime_U__HoldTimer[3] = {
        0,                   ACT(ConsumeEvent),       Eignore,
        0,                   ACT(ProduceHold),        Eignore,
        0,                   ACT(RestartHoldTimer),   EInHoldTime_U,
} ;

const STATE_ELEMENT InHoldTime_U__SuppressTimer[2] = {
        0,              ACT(ConsumeEvent),    Eignore,  // remember to consume event, otherwise state machine will loop...
        0,              ACT(ErrorEvent),      EInHoldTime_U
} ;


/**
 * All the rest of the events are errors so produce error entry to log and
 * stay in the InHoldTime_U state
 */
const STATE_ELEMENT InHoldTime_UErrorEvent[2] = {
        0,              ACT(ConsumeEvent),    Eignore,  // remember to consume event, otherwise state machine will loop...
        0,              ACT(ErrorEvent),      EInHoldTime_U
};

const STATE InHoldTime_U__[7] = {
        EDown,          InHoldTime_U__Down,
        EDrag,          InHoldTime_U__Drag,
        ECapacitiveUP,  InHoldTime_U__CapacitiveUp,
        EResistiveUP,   InHoldTime_U__ResistiveUp,
        ETouchTimer,    InHoldTime_UErrorEvent,
        EHoldTimer,     InHoldTime_U__HoldTimer,
        ESuppressTimer, InHoldTime_U__SuppressTimer
};


/*!
 *  :InHoldTime_D state end its elements
 *  Here drag, touch timer and suppress timer events are invalid
 */

const STATE_ELEMENT InHoldTime_D__Down[5] = {
        0,                    ACT(ClearSuppressTimer),    Eignore,
        0,                    ACT(ConsumeEvent),      Eignore,
        CND(InsideHoldArea),  0,                      EInHoldTime_U,
        0,                    ACT(ClearHoldTimer),    Eignore,
        0,                    ACT(ProduceMove),       EDispatch
} ;

/**
 * Note that ConsumeEvent is missing so after doing this the state engine will do InHoldTime_U
 */
const STATE_ELEMENT InHoldTime_D__CapacitiveUp[1] = {
        0,                  0,    EInHoldTime_U
} ;
/**
 * Note that ConsumeEvent is missing so after doing this the state engine will do InHoldTime_U
 */
const STATE_ELEMENT InHoldTime_D__ResistiveUp[1] = {
        0,                  0,    EInHoldTime_U     // InHoldTime_U initialises timers etc. if needed
} ;
/*!
 * In case of hold timer has been elapsed stop the timers, generate Release UI event.
 */
const STATE_ELEMENT InHoldTime_D__HoldTimer[4] = {
        0,                   ACT(ConsumeEvent),       Eignore,
        0,                   ACT(ClearSuppressTimer), Eignore,
        0,                   ACT(ClearHoldTimer),     Eignore,
        0,                   ACT(ProduceRelease),     EInit,
} ;
/*!
 * If suppress timer hits, stop the timers and generate Release UI event.
 */
const STATE_ELEMENT InHoldTime_D__SuppressTimer[4] = {
        0,                   ACT(ConsumeEvent),       Eignore,
        0,                   ACT(ClearSuppressTimer), Eignore,
        0,                   ACT(ClearHoldTimer),     Eignore,
        0,                   ACT(ProduceRelease),     EInit,
} ;

/**
 * All the rest of the events are errors so produce error entry to log and
 * stay in the InHoldTime_D state
 */
const STATE_ELEMENT InHoldTime_DErrorEvent[2] = {
        0,              ACT(ConsumeEvent),    Eignore,  // remember to consume event, otherwise state machine will loop...
        0,              ACT(ErrorEvent),      EInHoldTime_D
};

const STATE InHoldTime_D__[7] = {
        EDown,          InHoldTime_D__Down,
        EDrag,          InHoldTime_DErrorEvent,
        ECapacitiveUP,  InHoldTime_D__CapacitiveUp,
        EResistiveUP,   InHoldTime_D__ResistiveUp,
        ETouchTimer,    InHoldTime_DErrorEvent,
        EHoldTimer,     InHoldTime_D__HoldTimer,
        ESuppressTimer, InHoldTime_D__SuppressTimer
};


/*!
 *  :InTouchArea state end its elements
 *  Here Drag and Up events are valid.
 *  If drag is inside touch are it is ignored, otherwise
 *  the Dispatch state will handle the event.
 */

const STATE_ELEMENT InTouchArea__Drag[3] = {
        0,                  ACT(StoreMovePos),    Eignore,
        CND(InsideTouchArea), ACT(ConsumeEvent),     EInTouchArea,
        0,                    0,                     EDispatch  // Note that in this case ConsumeEvent has not been called so Dispatch state processes the message
} ;

/**
 * Note that ConsumeEvent is missing so after doing this the state engine will do Dispatch
 */
const STATE_ELEMENT InTouchArea__CapacitiveUp[1] = {
        0,                  0,    EDispatch
} ;
/**
 * Note that ConsumeEvent is missing so after doing this the state engine will do Dispatch
 */
const STATE_ELEMENT InTouchArea__ResistiveUp[1] = {
        0,                  0,    EDispatch
} ;

/**
 * All the rest of the events are errors so produce error entry to log and
 * stay in the InTouchArea state
 */
const STATE_ELEMENT InTouchAreaErrorEvent[2] = {
        0,              ACT(ConsumeEvent),    Eignore,  // remember to consume event, otherwise state machine will loop...
        0,              ACT(ErrorEvent),      EInTouchArea
};

const STATE InTouchArea__[7] = {
        EDown,          InTouchAreaErrorEvent,
        EDrag,          InTouchArea__Drag,
        ECapacitiveUP,  InTouchArea__CapacitiveUp,
        EResistiveUP,   InTouchArea__ResistiveUp,
        ETouchTimer,    InTouchAreaErrorEvent,
        EHoldTimer,     InTouchAreaErrorEvent,
        ESuppressTimer, InTouchAreaErrorEvent
};


/*!
 *  :Suppress_D state end its elements
 *  Here Down and suppress timers are OK.
 */

/*!
 * Down will be handled as a Drag event in the Dispatch state.
 */
const STATE_ELEMENT Suppress_D__Down[4] = {
        0,           ACT(ClearSuppressTimer),    Eignore,
        0,           ACT(RenameToDrag),          EDispatch
} ;
/*!
 * Suppress timer will generate Release UI event.
 */
const STATE_ELEMENT Suppress_D__SuppressTimer[3] = {
        0,                   ACT(ConsumeEvent),       Eignore,
        0,                   ACT(ClearSuppressTimer), Eignore,
        0,                   ACT(ProduceRelease),     EInit,
} ;

/**
 * All the rest of the events are errors so produce error entry to log and
 * stay in the Suppress_D state
 */
const STATE_ELEMENT Suppress_DErrorEvent[2] = {
        0,              ACT(ConsumeEvent),    Eignore,  // remember to consume event, otherwise state machine will loop...
        0,              ACT(ErrorEvent),      ESuppress_D
};

const STATE Suppress_D__[7] = {
        EDown,          Suppress_D__Down,
        EDrag,          Suppress_DErrorEvent,
        ECapacitiveUP,  Suppress_DErrorEvent,
        EResistiveUP,   Suppress_DErrorEvent,
        ETouchTimer,    Suppress_DErrorEvent,
        EHoldTimer,     Suppress_DErrorEvent,
        ESuppressTimer, Suppress_D__SuppressTimer
};
/*!
 * The allStates array contains all the possible states of the state machine.
 */
const STATE* const allStates[8] =
{
        Ignore__,
        Init__,
        Dispatch__,
        InTouchTime__,
        InHoldTime_U__,
        InHoldTime_D__,
        InTouchArea__,
        Suppress_D__
};
/*!
 * stateNames are used in the logging
 */
#ifdef GESTURE_LOGGING
const char* const stateNames[8] =
{
        "Ignore",
        "Init",
        "Dispatch",
        "InTouchTime",
        "InHoldTime_U",
        "InHoldTime_D",
        "InTouchArea",
        "Suppress"
};

// event names are also used in logging
const char* const hweventNames[] = {
        "EDown",
        "EDrag",
        "ECapacitiveUP",
        "EResistiveUP",
        "ETouchTimer",
        "EHoldTimer",
        "ESuppressTimer"
} ;
#endif

/*! CStateEngine contains the methods used in the state machine implementation.
 *
 *  The methods in CStateEngine used in the state machine definition are
 *  either condition methods or action methods.
 *
 *  Constructor
 *  \param[in]: MTimerInterface atimerif.  An attempt to make this more OS agnostic the actual
 *  timers are accessed using a separate interface.
 */
CStateEngine::CStateEngine(CStateEngineConfiguration* aConfig, MTimerInterface* atimerif, int aIndex)
{
    m_config = aConfig ;
    m_timerif = atimerif ;
    m_currentState = EInit ;
    m_index = aIndex ;
    isStatemachineBlocked = false;
}

CStateEngine::~CStateEngine()
{
    // Just to be sure...
    iTouchPoints.Reset() ;
    iDragPoints.ResetAndDestroy() ;
}
/*!
 * ConsumeEvent: the method defines that the turnStateMachine will stop the processing
 * of the state methods after it has reached the next state.
 *
 */
void CStateEngine::ConsumeEvent()
{
    m_eventConsumed = true ;
}
/*!
 * Condition method
 * \return true, if the touch timer limit > 0
 */
bool CStateEngine::IsTouchTimer()
{
    bool isit =  (m_config->m_touchTimerLimit > 0) ;

    return isit ;
}
/*!
 * Condition method
 * \return true, if the hold timer limit > 0
 */
bool CStateEngine::IsHoldTimer()
{
    bool isit =  (m_config->m_holdTimerLimit > 0) ;

    return isit ;
}
/*!
 * Condition method
 * \return true, if the suppress timer limit > 0
 */
bool CStateEngine::IsSuppressTimer()
{
    bool isit =  (m_config->m_suppressTimerLimit > 0) ;

    return isit ;
}
/*!
 * Condition method
 * \return true, if the touch area has been defined (the touch tolerancelength > 0)
 */
bool CStateEngine::IsTouchTimeArea()
{
    bool isit = (m_config->m_touchTimeTolerance.iX > 0) ;
    return isit  ;
}
/*!
 * Condition method
 * \return true, if the touch area has been defined (the touch tolerancelength > 0)
 */
bool CStateEngine::IsTouchArea()
{
    bool isit = (m_config->m_touchTolerance.iX > 0) ;
    return isit  ;
}
/*!
 * Condition method
 * \return true, if the hold area has been defined (the hold tolerancelength > 0)
 */
bool CStateEngine::IsHoldArea()
{
    bool isit = (m_config->m_holdTolerance.iX > 0) ;
    return isit  ;
}

bool CStateEngine::InsideArea(const TPoint& point,
                              const TRect& rect,
                              TAreaShape shape,
                              const TPoint& tolerance)
{
    bool isit;
    switch(shape)
    {
    default:    // pass trough
    case ERectangle:
    {
        isit = rect.Contains(m_hwe.iPosition) ;
        break ;
    }
    case ECircle:
    {
        TPoint delta = m_hwe.iPosition - point;
        long circlepoint = delta.iX * delta.iX + delta.iY * delta.iY;
        isit = (circlepoint < tolerance.iX * tolerance.iX);
        break ;
    }
    case EEllipse:
    {
        int asquare = tolerance.iX * tolerance.iX ;
        int bsquare = tolerance.iY * tolerance.iY ;
        TPoint delta = m_hwe.iPosition - point;
        int result = (delta.iX * delta.iX) * bsquare + (delta.iY * delta.iY) * asquare;

        isit = (result < asquare * bsquare);
        break ;
    }
    }
    return isit ;
}

/*!
 * Condition method
 * Check if the current event is positioned inside the touch area.
 * Touch area can be a rectangle, a circle or an ellipse, so different
 * calculation needs to be done based on the shape of the area.
 */
bool CStateEngine::InsideTouchTimeArea()
{
    return InsideArea(m_touchCentre, m_touchRect,
                      m_config->m_touchAreaShape, m_config->m_touchTimeTolerance);
}
/*!
 * Condition method
 * Check if the current event is positioned inside the touch area.
 * Touch area can be a rectangle, a circle or an ellipse, so different
 * calculation needs to be done based on the shape of the area.
 */
bool CStateEngine::InsideTouchArea()
{
    return InsideArea(m_touchCentre, m_touchRect,
                      m_config->m_touchAreaShape, m_config->m_touchTolerance);
}
/*!
 * Condition method
 * Check if the current event is positioned inside the hold area.
 * Hold area can be a rectangle, a circle or an ellipse, so different
 * calculation needs to be done based on the shape of the area.
 */
bool CStateEngine::InsideHoldArea()
{
    return InsideArea(m_holdCentre, m_holdRect,
                      m_config->m_holdAreaShape, m_config->m_holdTolerance);
}
/*!
 * Condition method
 * Check if the gesture looks like a hold, i.e. the movement has stopped.
 * \sa isNewHoldingPoint
 */
bool CStateEngine::LooksLikeHold()
{
    bool isit = isNewHoldingPoint() ;
    return isit ;
}
/*!
 * Action method
 * Error logging.
 */
void CStateEngine::ErrorEvent()
{
    // Log the error
    if (m_config->m_enableLogging)
    {
        LOGARG("ErrorEvent: %s %s", stateNames[m_currentState], hweventNames[m_hwe.iType]) ;
    }
}
/*!
 * Action method
 * Initialize touch timer.  At the same time calculate also the touch rectangle.
 */
void CStateEngine::InitTouchTimer()
{
    m_touchRect = ToleranceRect(m_hwe.iPosition, m_config->m_touchTolerance) ;
    m_touchCentre = m_hwe.iPosition ;
    m_timerif->startTouchTimer(m_config->m_touchTimerLimit, m_index) ;
}
/*!
 * Action method.
 * Initialize hold timer.  At the same time calculate also the hold rectangle.
 */
void CStateEngine::InitHoldTimer()
{
    m_holdRect = ToleranceRect(m_hwe.iPosition, m_config->m_holdTolerance) ;
    m_holdCentre = m_hwe.iPosition ;
    m_timerif->startHoldTimer(m_config->m_holdTimerLimit, m_index) ;
}
/*!
 * Action method
 * Restart the hold timer using the hold timer limit.
 */
void CStateEngine::RestartHoldTimer()
{
    m_timerif->startHoldTimer(m_config->m_holdTimerLimit, m_index) ;
}
/*!
 * Action method
 * Initialize suppression timer.  This timer is used during touch detection when
 * resistive UP has been detected.  If new DOWN comes while timer is running, it is ignored.
 */
void CStateEngine::InitTouchSuppressTimer()
{
    m_timerif->startSuppressTimer(m_config->m_suppressTimerLimit, m_index) ;
}
/*!
 * Action method.
 * Initialize suppression timer after move.  Tests show that when user is using light touch and
 * moving finger to opposite directions there may be accidental ups and downs where the time between
 * up and down may be well over 120 ms.
 */
void CStateEngine::InitMoveSuppressTimer()
{
    m_timerif->startSuppressTimer(m_config->m_moveSuppressTimerLimit, m_index) ;
}
/*!
 * Action method
 * Stop the touch timer.
 */
void CStateEngine::ClearTouchTimer()
{
    m_timerif->cancelTouchTimer(m_index) ;
}
/*!
 * Action method
 * Stop the hold timer.
 */
void CStateEngine::ClearHoldTimer()
{
    m_timerif->cancelHoldTimer(m_index) ;
}
/*!
 * Action method
 * Stop the suppress timer.
 */
void CStateEngine::ClearSuppressTimer()
{
    m_timerif->cancelSuppressTimer(m_index) ;
}
/*!Helper method.
 * Create UI event
 * \param code The new UI event type (Touch, Release, Move, Hold)
 */
CUiEvent* CStateEngine::createUIEventL(TUiEventCode code, const TPoint& aPos)
{

    m_previousUiGenerated = code ;
    return CUiEvent::NewL(code, m_gestureStartXY, aPos, getPreviousXY(aPos),
        isTimerMessage(), m_hwe.iTarget, getInterval(), m_index, m_hwe.iTime.Int64()) ;
}
/*!
 * Return the previous XY position and store the current for next round
 */
TPoint CStateEngine::getPreviousXY(const TPoint& aCurrentXY)
{
    TPoint p = m_previousXY ;
    m_previousXY = aCurrentXY ;
    return p ;
}
/*!
 * \return true, if the current event was timer triggered
 */
bool CStateEngine::isTimerMessage()
{
    return (m_hwe.iType >= ETouchTimer); // NOTE: if new events are added at the end of the list this needs to be changed
}
/*!
 * Action method.
 * Generate the Touch UI event.
 * If there are a set of touch points collected, calculate the position to the
 * Touch UI event to be the average of the collected points.
 */
void CStateEngine::ProduceTouch()
{
    m_wasFiltered = false ;
    CUiEvent* cue = NULL;
    getInterval() ; // dummy call to initialize the variable....
    TInt err(KErrNone);
    if (iTouchPoints.Count()>0)
    {
        // calculate average of the touch points
        m_currentTouchXY = calculateTouchAverageFromPoints() ;
        TRAP(err, cue = createUIEventL(stmUiEventEngine::ETouch, m_currentTouchXY)) ;
    }
    else
    {
        TRAP(err, cue = createUIEventL(stmUiEventEngine::ETouch, m_uiEventXY)) ;
    }
    if(!err)
        m_config->m_uiEventSender->AddEvent(cue) ;
}
/*!
 * Action method
 * Generate the Move UI event.  The position of the event has been set in the SetCurrentPos
 * The previous position needs some special handling, if filtering has been used.
 * \sa SetCurrentPos
 */
void CStateEngine::ProduceMove()
{
    m_wasFiltered = false ;
    if (m_uiEventXY == m_previousXY) {
    return;
    }
    CUiEvent* cue = NULL;
    TRAPD(err, cue = createUIEventL(stmUiEventEngine::EMove, m_uiEventXY)) ;

    if(!err)
    	m_config->m_uiEventSender->AddEvent(cue) ;
}
/*!
 * Action method
 * Generate the Release UI event.
 */
void CStateEngine::ProduceRelease()
{
    m_wasFiltered = false ;
    CUiEvent* cue = NULL;
    TRAPD(err, cue = createUIEventL(stmUiEventEngine::ERelease, m_uiEventXY)) ;
    if(!err)
    	m_config->m_uiEventSender->AddEvent(cue) ;

    if (m_config->m_enableLogging)
    {
        LOGFLUSH ;
    }
}
/*!
 * Action method
 * Generate the Hold UI event.
 */
void CStateEngine::ProduceHold()
{
    m_wasFiltered = false ;
    CUiEvent* cue = NULL;
    TRAPD(err, cue = createUIEventL(stmUiEventEngine::EHold, m_holdCentre)) ;
    if(!err)
 	   m_config->m_uiEventSender->AddEvent(cue) ;

}
/*!
 * Action method
 * Rename the current event to drag.  This is used when the accidental up/down message pair
 * has been detected, the DOWN event is handled as it were a move event.
 */
void CStateEngine::RenameToDrag()
{
    m_hwe.iType = stmUiEventEngine::EDrag ;
}
/*!
 * Action method
 * Initialize the touch time area.  Clear the array for collected touch points and
 * calculate the touch rectangle.
 */
void CStateEngine::PrepareTouchTimeArea()
{
    if (iTouchPoints.Count()>0) iTouchPoints.Reset() ;
    m_touchRect = ToleranceRect(m_hwe.iPosition, m_config->m_touchTimeTolerance) ;
}

/*!
 * Action method
 * Initialize the touch area.  Clear the array for collected touch points and
 * calculate the touch rectangle.
 */
void CStateEngine::PrepareTouchArea()
{
    if (iTouchPoints.Count()>0) iTouchPoints.Reset() ;
    m_touchRect = ToleranceRect(m_hwe.iPosition, m_config->m_touchTolerance) ;
}
/*!
 * Action method
 * Initialize the hold area rectangle.
 */
void CStateEngine::PrepareHoldArea()
{
    m_holdRect = ToleranceRect(m_hwe.iPosition, m_config->m_holdTolerance) ;
}
/*!
 * Action method
 * Store the current position and time always when we see EDrag.  The stored value is used
 * to calculate correct speed after filtered messages.
 */
void CStateEngine::StoreMovePos()
{
    if (m_config->m_enableLogging)
    {
        LOGARG("store move pos from (%d, %d) to (%d, %d)",
                m_lastFilteredPosition.iX, m_lastFilteredPosition.iY,m_hwe.iPosition.iX,
                m_hwe.iPosition.iY  ) ;
    }
    m_lastFilteredPosition = m_hwe.iPosition ;
    m_lastFilteredMessageTime = m_hwe.iTime ;

}
/*!
 * Action method
 * Store the current position and time.
 */
void CStateEngine::SetCurrentPos()
{
    m_uiEventXY = m_hwe.iPosition ;
}
/*!
 * Action method
 * Initialize the gesture starting.
 */
void CStateEngine::SetGestureStart()
{
    m_gestureStartXY = m_hwe.iPosition ;
    m_previousXY = m_hwe.iPosition ;
    m_gestureTarget = m_hwe.iTarget ;
    iDragPoints.ResetAndDestroy() ;
    iTouchPoints.Reset() ;
}
/*!
 * Action method
 * Add current point to the set of touch points.
 */
void CStateEngine::AddToTouch()
{
    iTouchPoints.Append(THwEvent(m_hwe.iType,
                                 m_hwe.iPosition,
                                 m_hwe.iTime,
                                 m_hwe.iTarget,
                                 m_index)
                        ) ;
    // calculate the average of touch points and move the touch area accordingly
    // this allows slight movement of the figertip while inside touch time
    if (iTouchPoints.Count()>2)
    {
        TPoint newtp = calculateTouchAverageFromPoints() ;
        m_touchRect = ToleranceRect(newtp, m_config->m_touchTolerance) ;
        m_holdRect = ToleranceRect(newtp, m_config->m_holdTolerance) ;
    }
}
/*!
 * Action method
 * Add the current point to the set of dragging points.
 * The set of dragging points is examined to determine if a enw hold has been started.
 */
void CStateEngine::AddDraggingPos()
{
    iDragPoints.Append(new THwEvent(m_hwe.iType,
                                    m_hwe.iPosition,
                                    m_hwe.iTime,
                                    m_hwe.iTarget,
                                    m_index)
                       ) ;
}
/*!
 * HandleStateEvent processes one event, which can be either pointer event or timer event.
 * The event is handled by calling the turnStateMachine method.
 */
bool CStateEngine::handleStateEvent()
{
    // We get an event into m_hwe by this moment, lets kick the state machine
    m_wasFiltered = ETrue ;
    if (isStatemachineBlocked)
        return m_wasFiltered;

    CalculateDelta() ;
    turnStateMachine();

    m_previousPointerEventPosition = m_hwe.iPosition ;
    return m_wasFiltered ;
}

/*!
 *  Get the current touch rectangle.  If touch state not currently on, returns TRect(TPoint(0,0),TPoint(0,0))
 * (touch state meaning that the touch timer is still running and the points have been kept inside the area)
 */
TRect CStateEngine::getTouchArea()
{
    return m_touchRect ;
}
/*!
 * get the hold area rectangle
 */
TRect CStateEngine::getHoldArea()
{
    return m_holdRect ;
}
/*!
 * MStateMachine method.
 */
bool CStateEngine::wasLastMessageFiltered()
{
    return m_wasFiltered ;
}

/*!
 * Check if the last X points in the stored points look like the movement has stopped
 */
bool CStateEngine::isNewHoldingPoint()
{
    int x = iDragPoints.Count();
    if (x > 2)    // are there any points to be checked?
    {
        THwEvent* phwe = iDragPoints[x-1] ;
        THwEvent* phweinsidehold = phwe ;
        TRect recth = ToleranceRect(phwe->iPosition, m_config->m_holdTolerance) ;
        // Look backwards from the last point to see if there are enought points (enough in time) to look like a hold
        x -= 2 ;
        while (x > 0 && recth.Contains(iDragPoints[x]->iPosition))
        {
            phweinsidehold = iDragPoints[x];
            --x;
        }
        TTimeIntervalMicroSeconds tival = phwe->iTime.MicroSecondsFrom(phweinsidehold->iTime) ;

        /**
         * remove the extra points from the list if they are outside of holding area
         */
        while (x > 0)
        {
            THwEvent* p = iDragPoints[x] ;
            delete p ;
            iDragPoints.Remove(x) ;
            --x ;
        }

        // See the time difference of the two points which still are inside the hold area
        TTimeIntervalMicroSeconds limit = m_config->m_holdTimerLimit/2 ;
        if (tival > limit)
        {
            if (m_config->m_enableLogging)
            {
                LOGARG("isNewHoldingPoint: %s, dragpoints count %d",
                        stateNames[m_currentState], iDragPoints.Count()) ;
            }
            return true ;
        }
    }
    else
    {
        // one or 0 points does not look like hold

    }
    return false ;
}
/*!
 * calculate simple average of the touch points, i.e. calculate the average of the previous and current
 * position.  Note that the touch point remains the same, this just calculates new value for the UI position
 */
void CStateEngine::CalculateTouchAverage()
{
    m_uiEventXY.iX =  (m_uiEventXY.iX+m_hwe.iPosition.iX)/2 ;
    m_uiEventXY.iY =  (m_uiEventXY.iY+m_hwe.iPosition.iY)/2 ;
}
/*!
 * Calculate the movement vector.
 */
void CStateEngine::CalculateDelta()
{
    m_deltaVector.iX = m_hwe.iPosition.iX-m_previousPointerEventPosition.iX ;
    m_deltaVector.iY = m_hwe.iPosition.iY-m_previousPointerEventPosition.iY ;
}
/*!internal
 * Debug logging method
 */
void CStateEngine::DebugPrintState(TStateMachineState anextstate)
{
    if (m_config->m_enableLogging)
    {
    LOGARG("%s: cuiev(%d,%d) cTxy ((%d,%d)(%d,%d)) cHxy ((%d,%d)(%d,%d)) gsXY(%d,%d) dV(%d,%d) EVNT(%d,%d (%s)) going to %s",
            stateNames[m_currentState],
            m_uiEventXY.iX, m_uiEventXY.iY,
            m_touchRect.iTl.iX, m_touchRect.iTl.iY,m_touchRect.iBr.iX, m_touchRect.iBr.iY,
            m_holdRect.iTl.iX, m_holdRect.iTl.iY,m_holdRect.iBr.iX, m_holdRect.iBr.iY,
            m_gestureStartXY.iX, m_gestureStartXY.iY,
            m_deltaVector.iX, m_deltaVector.iY,
            m_hwe.iPosition.iX, m_hwe.iPosition.iY, hweventNames[m_hwe.iType],
            stateNames[anextstate]
            );
    }
}

/*!
 * calculate the rectangle for touch or hold
 */
TRect CStateEngine::ToleranceRect(const TPoint& aCenterPoint, const TPoint& tolerance)
{
    // grow by the tolerance length, while keeping the center point
    TRect toleranceRect(
        aCenterPoint - tolerance,
        aCenterPoint + tolerance);
    return toleranceRect;
}
/*!
 * turnStateMachine.  Go trough the state elements found for the current event
 * until the event has been consumed.
 *
 * \pre m_currentState defines the current state and the index to the allStates array.
 * \pre m_hwe is the message being handled.  The corresponding STATE_ELEMENT array must be found and processed.
 *
 */
void CStateEngine::turnStateMachine()
{

    const STATE_ELEMENT* pelement ;
    m_eventConsumed = false ;   // run the loop until the event has been consumed
    // Now run trough the motions of the state elements, and prepare to change to next state while doing so.
    // If the state elements set the m_eventConsumed then all is done
    isStatemachineBlocked = true;
    while (!m_eventConsumed)
    {
        int i = 0 ;
        const STATE* const pcurrentstate = allStates[m_currentState] ;
        // Since each state definition must contain entries for all possible events the following loop cannot fail ;-)
        while (pcurrentstate[i].theEvent != m_hwe.iType ) ++i ;
        pelement = pcurrentstate[i].stateElements ;
        TStateMachineState      nextState = Eignore ;
        /*
         * Handle the individual state elements.  If there is a condition function,
         * call the function and if it returns true, handle the action function and possible next state
         * if the condition returns false, continue to next element
         * if there is no condition, run the action function if it exists.
         * if the next state is defined (i.e it is != Eignore), go to that state
         */
        while (nextState == Eignore)
        {
            condition_t cndfunc = pelement->conditionFunction ;
            action_t    actfunc = pelement->actionFunction ;
            if (cndfunc != 0)
            {
                /*
                 * There was a condition function, call it to see whether the action needs to performed and/or the next satte defined
                 */
                if (cndfunc(this))
                {
                    // Condition was true, handle it
                    // call the action if it exists
                    if (actfunc != 0) actfunc(this) ;
                    // and now get to the next state
                    nextState   = pelement->nextState ; // Note that while this remains Eignore there are elements to be run
                }
            }
            else
            {
                /**
                 * No condition function, call the possible action function and get the next state
                 */
                if (actfunc != 0) actfunc(this) ;
                nextState   = pelement->nextState ;     // Note that while this remains Eignore there are elements to be run
            }
            ++pelement ;    // next entry in the elements
        }
        if (m_config->m_enableLogging) DebugPrintState(nextState) ;
        m_currentState = nextState ;    // Change to the next state
    }
    isStatemachineBlocked = false;
}
TTimeIntervalMicroSeconds CStateEngine::getInterval()
{
    TTime now ;
    now.HomeTime() ;
    TTimeIntervalMicroSeconds interval = now.MicroSecondsFrom(m_lastMessageTime) ;
    m_lastMessageTime = now ;
    return interval ;
}

TPoint CStateEngine::calculateTouchAverageFromPoints()
{
    TPoint tp ;
    int count = iTouchPoints.Count() ;
    for (int i = 0; i < count; i++)
    {
        tp += iTouchPoints[i].iPosition;
    }
    if(count)
        {
        tp.iX /= count ;
        tp.iY /= count ;
        }
    return tp ;
}

