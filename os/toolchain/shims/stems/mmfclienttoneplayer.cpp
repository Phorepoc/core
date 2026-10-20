// Copyright (c) 2002-2009 Nokia Corporation and/or its subsidiary(-ies).
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
//

#include <mmf/common/mmfpaniccodes.h>

#include "mmfclienttoneplayer.h"
using namespace ContentAccess;
enum TMmfMdaAudioToneUtility
	{
	EBadArgument,
	EPostConditionViolation, 
	EPlayStartedCalledWithError
	};

void Panic(TInt aPanicCode)
	{
	_LIT(KMMFMediaClientAudioPanicCategory, "Stem_MMFAudioClient");
	User::Panic(KMMFMediaClientAudioPanicCategory, aPanicCode);
	}

// Dummy DevSound class

CDummyDevSound::CDummyDevSound()
	: CTimer(EPriorityStandard)
	{}

CDummyDevSound* CDummyDevSound::NewL()
	{
	CDummyDevSound* self = new(ELeave) CDummyDevSound();
	// OURS fix to the stemkit stem: the timer was never constructed nor added to the active
	// scheduler, so the first After() (= the first key click) panicked E32USER-CBase 51
	CleanupStack::PushL(self);
	self->ConstructL();
	CActiveScheduler::Add(self);
	CleanupStack::Pop(self);
	return self;
	}

void CDummyDevSound::InitializeL(MDevSoundObserver& aDevSoundObserver)
	{
	iObserver = &aDevSoundObserver;
	iObserver->InitializeComplete(KErrNone);
	}

void CDummyDevSound::Play(const TTimeIntervalMicroSeconds& aDuration)
	{
	if (IsActive())
		{
		// currently playing - ignore the request?
		return;
		}
	TTimeIntervalMicroSeconds32 d = I64LOW(aDuration.Int64());
	if (d <= TTimeIntervalMicroSeconds32(0))
		{
		d = 10;
		}
	After(d);
	}

void CDummyDevSound::RunL()
	{
	RDebug::Printf("!Beep!\n");
	iObserver->ToneFinished(KErrNone);
	}

/**
Creates a new instance of the tone player utility.
The default  volume is set to MaxVolume() / 2.

@param  aObserver
        A class to receive notifications from the tone player.
@param  aServer
        This parameter is no longer used and should be NULL.

@return A pointer to the new audio tone player utility object.

@since 5.0
*/
EXPORT_C CMdaAudioToneUtility* CMdaAudioToneUtility::NewL(MMdaAudioToneObserver& aObserver, CMdaServer* aServer /*= NULL*/)
	{
	return CMdaAudioToneUtility::NewL(aObserver, aServer, EMdaPriorityNormal, EMdaPriorityPreferenceTimeAndQuality);
	}

/**
Creates a new instance of the tone player utility.
The default  volume is set to MaxVolume() / 2.

@param  aObserver
        A class to receive notifications from the tone player
@param  aServer
        This parameter is no longer used and should be NULL
@param  aPriority
        The Priority Value - this client's relative priority. This is a value between EMdaPriorityMin and 
        EMdaPriorityMax and represents a relative priority. A higher value indicates a more important request.
@param  aPref
        The Priority Preference - an additional audio policy parameter. The suggested default is 
        EMdaPriorityPreferenceNone. Further values are given by TMdaPriorityPreference, and additional 
        values may be supported by given phones and/or platforms, but should not be depended upon by 
        portable code.

@return A pointer to the new audio tone player utility object.

@since 5.0

Note: The Priority Value and Priority Preference are used primarily when deciding what to do when
several audio clients attempt to play or record simultaneously. In addition to the Priority Value and Preference, 
the adaptation may consider other parameters such as the SecureId and Capabilities of the client process. 
Whatever, the decision  as to what to do in such situations is up to the audio adaptation, and may
vary between different phones. Portable applications are advised not to assume any specific behaviour. 
*/
EXPORT_C CMdaAudioToneUtility* CMdaAudioToneUtility::NewL(MMdaAudioToneObserver& aObserver, CMdaServer* /*aServer = NULL*/,
														  TInt aPriority /*= EMdaPriorityNormal*/,
														  TInt aPref /*= EMdaPriorityPreferenceTimeAndQuality*/)
	{
	CMdaAudioToneUtility* self = new(ELeave) CMdaAudioToneUtility();
	CleanupStack::PushL(self);
	self->iProperties = CMMFMdaAudioToneUtility::NewL(aObserver, NULL, aPriority, aPref);
	CleanupStack::Pop(); //self
	return self;
	}

/**
Destructor. Frees any resources held by the tone player

@since 5.0
*/
CMdaAudioToneUtility::~CMdaAudioToneUtility()
	{
	delete iProperties;
	}

/**
Returns the current state of the audio tone utility.

@return The state of the audio tone utility.

@since  5.0
*/
TMdaAudioToneUtilityState CMdaAudioToneUtility::State()
	{
	ASSERT(iProperties);
	return iProperties->State();
	}
	
/**
Returns the maximum volume supported by the device. This is the maximum value which can be 
passed to CMdaAudioToneUtility::SetVolume().

@return The maximum volume. This value is platform dependent but is always greater than or equal to one.

@since  5.0
*/
TInt CMdaAudioToneUtility::MaxVolume()
	{
	ASSERT(iProperties);
	return iProperties->MaxVolume();
	}
	
/**
Returns an integer representing the current volume of the audio device.

@return The current volume.

@since 		5.0
*/
TInt CMdaAudioToneUtility::Volume()
	{
	ASSERT(iProperties);
	return iProperties->Volume();
	}
	
/**
Changes the volume of the audio device.

The volume can be changed before or during play and is effective
immediately.

@param  aVolume
        The volume setting. This can be any value from zero to
        the value returned by a call to
        CMdaAudioToneUtility::MaxVolume().
        Setting a zero value mutes the sound. Setting the
        maximum value results in the loudest possible sound.

@since  5.0
*/
void CMdaAudioToneUtility::SetVolume(TInt aVolume)
	{
	ASSERT(iProperties);
	iProperties->SetVolume(aVolume);
	}
	
/**
Changes the clients priority.

@param  aPriority
        The Priority Value.
@param  aPref
        The Priority Preference.

@see CMdaAudioToneUtility::NewL()

@since  5.0

*/
void CMdaAudioToneUtility::SetPriority(TInt aPriority, TInt aPref)
	{
	ASSERT(iProperties);
	iProperties->SetPriority(aPriority, aPref);
	}

/**
Changes the duration of DTMF tones, the gaps between DTMF tones and the
pauses.

@param  aToneLength
        The duration of the DTMF tone in microseconds.
@param  aToneOffLength
        The gap between DTFM tones in microseconds.
@param  aPauseLength
        Pauses in microseconds
*/
void CMdaAudioToneUtility::SetDTMFLengths(TTimeIntervalMicroSeconds32 aToneLength,
										  TTimeIntervalMicroSeconds32 aToneOffLength,
										  TTimeIntervalMicroSeconds32 aPauseLength)
	{
	ASSERT(iProperties);
	iProperties->SetDTMFLengths(aToneLength, aToneOffLength, aPauseLength);
	}

/**
Sets the number of times the tone sequence is to be repeated during
the play operation.

A period of silence can follow each playing of the tone sequence. The
tone sequence can be repeated indefinitely.

@param  aRepeatNumberOfTimes
        The number of times the tone sequence, together with
        the trailing silence, is to be repeated. If this is
        set to KMdaRepeatForever, then the tone
        sequence, together with the trailing silence, is
        repeated indefinitely. The behaviour is undefined for values other than  
		KMdaRepeatForever, zero and positive.
@param  aTrailingSilence
        The time interval of the training silence. The behaviour is undefined
        for values other than zero and positive.

@since  5.0
*/
void CMdaAudioToneUtility::SetRepeats(TInt aRepeatNumberOfTimes,
									  const TTimeIntervalMicroSeconds& aTrailingSilence)
	{
	ASSERT(iProperties);
	iProperties->SetRepeats(aRepeatNumberOfTimes, aTrailingSilence);
	}

/**
Defines the period over which the volume level is to rise smoothly
from nothing to the normal volume level.

@param  aRampDuration
        The period over which the volume is to rise. A zero
        value causes the tone to be played at the normal level
        for the full duration of the playback. A value which
        is longer than the duration of the tone sequence means
        that the tone never reaches its normal volume level.

@since  5.0
*/
void CMdaAudioToneUtility::SetVolumeRamp(const TTimeIntervalMicroSeconds& aRampDuration)
	{
	ASSERT(iProperties);
	iProperties->SetVolumeRamp(aRampDuration);
	}

/**
Returns the number of available pre-defined tone sequences.

@return The number of tone sequences. This value is implementation 
		dependent but is always greater than or equal to zero.

@since  5.0
*/
TInt CMdaAudioToneUtility::FixedSequenceCount()
	{
	ASSERT(iProperties);
	return iProperties->FixedSequenceCount();
	}

/**
Returns the name assigned to a specific pre-defined tone sequence.

@param  aSequenceNumber
        The index identifying the specific pre-defined tone sequence. 
        Index values are relative to zero. This can be any value from 
        zero to the value returned by a call to FixedSequenceCount() - 1.
        The function raises a panic if sequence number is not within this
 		range.

@see CMMFDevSound::FixedSequenceName(TInt aSequenceNumber)
@see FixedSequenceCount()

@return The name assigned to the tone sequence.

@since  5.0
*/
const TDesC& CMdaAudioToneUtility::FixedSequenceName(TInt aSequenceNumber)
	{
	ASSERT(iProperties);
	return iProperties->FixedSequenceName(aSequenceNumber);
	}

/**
Configures the audio tone player utility to play a single tone.

This function is asynchronous. On completion, the observer callback
function MMdaAudioToneObserver::MatoPrepareComplete() is
called, indicating the success or failure of the configuration
operation.The configuration operation can be cancelled by calling
CMdaAudioToneUtility::CancelPrepare(). The configuration
operation cannot be started if a play operation is in progress.

@param     aFrequency
           The frequency (pitch) of the tone in Hz.
@param     aDuration
           The duration of the tone in microseconds.
@since     5.0
*/
void CMdaAudioToneUtility::PrepareToPlayTone(TInt aFrequency, const TTimeIntervalMicroSeconds& aDuration)
	{
	ASSERT(iProperties);
	iProperties->PrepareToPlayTone(aFrequency, aDuration);
	}

/**
Configures the audio tone player utility to play a dual tone.
The generated tone consists of two sine waves of different
frequencies summed together.

This function is asynchronous. On completion, the observer callback
function MMdaAudioToneObserver::MatoPrepareComplete() is
called, indicating the success or failure of the configuration
operation. The configuration operation can be cancelled by calling
CMdaAudioToneUtility::CancelPrepare(). The configuration
operation cannot be started if a play operation is in progress.

@param  aFrequencyOne
        The first frequency (pitch) of the tone.
@param  aFrequencyTwo
        The second frequency (pitch) of the tone.
@param  aDuration
        The duration of the tone in microseconds.

@since  7.0sy
*/
EXPORT_C void CMdaAudioToneUtility::PrepareToPlayDualTone(TInt aFrequencyOne, TInt aFrequencyTwo, const TTimeIntervalMicroSeconds& aDuration)
	{
	ASSERT(iProperties);
	iProperties->PrepareToPlayDualTone(aFrequencyOne, aFrequencyTwo, aDuration);
	}

/**
Configures the audio tone utility player to play a DTMF (Dual-Tone
Multi-Frequency) string.

This function is asynchronous. On completion, the observer callback
function MMdaAudioToneObserver::MatoPrepareComplete() is
called, indicating the success or failure of the configuration
operation. The configuration operation can be cancelled by calling
CMdaAudioToneUtility::CancelPrepare(). The configuration
operation cannot be started if a play operation is in progress.

@param  aDTMF
        A descriptor containing the DTMF string.

@since  5.0
*/
void CMdaAudioToneUtility::PrepareToPlayDTMFString(const TDesC& aDTMF)
	{
	ASSERT(iProperties);
	iProperties->PrepareToPlayDTMFString(aDTMF);
	}

/**
Configures the audio tone player utility to play a tone sequence
contained in a descriptor.

This function is asynchronous. On completion, the observer callback
function MMdaAudioToneObserver::MatoPrepareComplete() is
called, indicating the success or failure of the configuration
operation. The configuration operation can be cancelled by calling
CMdaAudioToneUtility::CancelPrepare(). The configuration
operation cannot be started if a play operation is in progress.

@param  aSequence
        The descriptor containing the tone sequence. The
        format of the data is unspecified but is expected to
        be platform dependent. A device might support more
        than one form of sequence data.

@since  5.0
*/
void CMdaAudioToneUtility::PrepareToPlayDesSequence(const TDesC8& aSequence)
	{
	ASSERT(iProperties);
	iProperties->PrepareToPlayDesSequence(aSequence);
	}

/**
Configures the audio tone player utility to play a tone sequence
contained in a file.

This function is asynchronous. On completion, the observer callback
function MMdaAudioToneObserver::MatoPrepareComplete() is
called, indicating the success or failure of the configuration
operation. The configuration operation can be cancelled by calling
CMdaAudioToneUtility::CancelPrepare(). The configuration
operation cannot be started if a play operation is in progress.

@param  aFileName
        The full path name of the file containing the tone
        sequence. The format of the data is unspecified but is
        expected to be platform dependent. A device might
        support more than one form of sequence data.

@since  5.0
*/
void CMdaAudioToneUtility::PrepareToPlayFileSequence(const TDesC& aFileName)
	{
	ASSERT(iProperties);
	iProperties->PrepareToPlayFileSequence(aFileName);
	}
	
/**
Configures the audio tone player utility to play a tone sequence
contained in a file.

This function is asynchronous. On completion, the observer callback
function MMdaAudioToneObserver::MatoPrepareComplete() is
called, indicating the success or failure of the configuration
operation. The configuration operation can be cancelled by calling
CMdaAudioToneUtility::CancelPrepare(). The configuration
operation cannot be started if a play operation is in progress.

@param  aFile
        A handle to an open file containing the tone
        sequence. The format of the data is unspecified but is
        expected to be platform dependent. A device might
        support more than one form of sequence data.

@since  5.0
*/
EXPORT_C void CMdaAudioToneUtility::PrepareToPlayFileSequence(RFile& aFile)
	{
	ASSERT(iProperties);
	iProperties->PrepareToPlayFileSequence(aFile);
	}
	

/**
Configures the audio tone player utility to play the specified
pre-defined tone sequence.

This function is asynchronous. On completion, the observer callback
function MMdaAudioToneObserver::MatoPrepareComplete() is
called, indicating the success or failure of the configuration
operation. The configuration operation can be cancelled by calling
CMdaAudioToneUtility::CancelPrepare(). The configuration
operation cannot be started if a play operation is in progress.

@param  aSequenceNumber
        An index into the set of pre-defined tone sequences.
        This can be any value from zero to the value returned by a 
        call to FixedSequenceCount() - 1.
        If the sequence number is not within this range, a panic will be 
        raised when Play() is called later.

@see FixedSequenceCount()
@see CMMFDevSound::PlayFixedSequenceL(TInt aSequenceNumber)

@since  5.0
*/
void CMdaAudioToneUtility::PrepareToPlayFixedSequence(TInt aSequenceNumber)
	{
	ASSERT(iProperties);
	iProperties->PrepareToPlayFixedSequence(aSequenceNumber);
	}

/**
Cancels the configuration operation.

The observer callback function
MMdaAudioToneObserver::MatoPrepareComplete() is not
called.

@since  5.0
*/
void CMdaAudioToneUtility::CancelPrepare()
	{
	ASSERT(iProperties);
	iProperties->CancelPrepare();
	}

/**
Plays the tone.

The tone played depends on the current configuration.This function is
asynchronous. On completion, the observer callback function
MMdaAudioToneObserver::MatoPlayComplete() is called,
indicating the success or failure of the play operation.The play
operation can be cancelled by
calling CMdaAudioToneUtility::CancelPlay().

@since  5.0
*/
void CMdaAudioToneUtility::Play()
	{
	ASSERT(iProperties);
	iProperties->Play();
	}

EXPORT_C TInt CMdaAudioToneUtility::Pause()
	{
	ASSERT(iProperties);
	return iProperties->Pause();
	}

EXPORT_C TInt CMdaAudioToneUtility::Resume()
	{
	ASSERT(iProperties);
	return iProperties->Resume();
	}

/**
Cancels the tone playing operation.

The observer callback
function MMdaAudioToneObserver::MatoPlayComplete() is not
called.

@since  5.0
*/
void CMdaAudioToneUtility::CancelPlay()
	{
	ASSERT(iProperties);
	iProperties->CancelPlay();
	}

/**
Sets the stereo balance for playback.

@param 	aBalance
        The balance. Should be between KMMFBalanceMaxLeft and KMMFBalanceMaxRight.

@return An error code indicating if the function call was successful. KErrNone on success, otherwise
        another of the system-wide error codes.

@since 7.0s
*/
EXPORT_C void CMdaAudioToneUtility::SetBalanceL(TInt aBalance /*=KMMFBalanceCenter*/)
	{
	ASSERT(iProperties);
	iProperties->SetBalanceL(aBalance);
	}

/**
 *	Returns The current playback balance.This function may not return the same value 
 *			as passed to SetBalanceL depending on the internal implementation in 
 *			the underlying components.
 *
 *	@return The balance. Should be between KMMFBalanceMaxLeft and KMMFBalanceMaxRight.
 *		
 *  @since 	7.0s
 */
EXPORT_C TInt CMdaAudioToneUtility::GetBalanceL()
	{
	ASSERT(iProperties);
	return iProperties->GetBalanceL();
	}
	
/**
Retrieves a custom interface to the underlying device.

@param  aInterfaceId
        The interface UID, defined with the custom interface.

@return A pointer to the interface implementation, or NULL if the device does not
        implement the interface requested. The return value must be cast to the
        correct type by the user.
*/
EXPORT_C TAny* CMdaAudioToneUtility::CustomInterface(TUid aInterfaceId)
	{
	ASSERT(iProperties);
	return 0;
	}

EXPORT_C void CMdaAudioToneUtility::RegisterPlayStartCallback(MMdaAudioTonePlayStartObserver& aObserver)
	{
	ASSERT(iProperties);
	iProperties->RegisterPlayStartCallback(aObserver);
	}



CMMFMdaAudioToneUtility* CMMFMdaAudioToneUtility::NewL(MMdaAudioToneObserver& aObserver, CMdaServer* /*aServer = NULL*/,
														  TInt aPriority /*= EMdaPriorityNormal*/, 
														  TInt aPref /*= EMdaPriorityPreferenceTimeAndQuality*/)
														  
	{
	CMMFMdaAudioToneUtility* self = new(ELeave) CMMFMdaAudioToneUtility(aObserver, aPriority, aPref);
	CleanupStack::PushL(self);
	self->ConstructL();
	CleanupStack::Pop(self);
	return self;
	}



CMMFMdaAudioToneUtility::CMMFMdaAudioToneUtility(MMdaAudioToneObserver& aCallback, TInt aPriority, TInt aPref) :
	iCallback(aCallback)
	{
	iPrioritySettings.iPref = aPref;
	iPrioritySettings.iPriority = aPriority;
	iState = EMdaAudioToneUtilityNotReady;
	iInitialized = EFalse;
	iPlayCalled = EFalse;

#ifdef _DEBUG
	iPlayCalledBeforeInitialized = EFalse;
#endif
	}

void CMMFMdaAudioToneUtility::ConstructL()
	{
	iAsyncCallback = CMMFMdaAudioToneObserverCallback::NewL(*this, *this);

	// iDevSound = CMMFDevSound::NewL();
	// iDevSound->InitializeL(*this,EMMFStateTonePlaying);
	
	iTimer = CDummyDevSound::NewL();
	iTimer->InitializeL(*this);

	SetVolume(MaxVolume()/2 ); // set the volume to an intermediate value 
	}

CMMFMdaAudioToneUtility::~CMMFMdaAudioToneUtility()
	{
	delete iAsyncCallback;
	}



void CMMFMdaAudioToneUtility::InitializeComplete(TInt aError)
	{
#ifdef _DEBUG
	__ASSERT_ALWAYS(!iPlayCalledBeforeInitialized, User::Panic(_L("PlayInitialized called before InitializeComplete"), 0));
#endif
	iInitialized = ETrue;

	if (iPlayCalled)
		{
		// Play() is called before InitializeComplete()
		if (aError == KErrNone)
			{
			PlayAfterInitialized();
 			}
 		else 
 			{
 			// InitializeComplete() with error other than KErrNone
			iState = EMdaAudioToneUtilityNotReady;
			iAsyncCallback->MatoPlayComplete(aError);
 			}
 		iPlayCalled = EFalse;
		}
 	iInitializeState = aError;
	}

void CMMFMdaAudioToneUtility::ToneFinished(TInt aError)
	{
	if (aError != KErrCancel)
		{
		if (aError == KErrUnderflow)
			{
			aError = KErrNone;
			}

		iAsyncCallback->MatoPlayComplete(aError);
		}
	// else don't want to callback after a cancel
	}


TMdaAudioToneUtilityState CMMFMdaAudioToneUtility::State()
	{
	return iState;
	}

TInt CMMFMdaAudioToneUtility::MaxVolume()
	{
	return 100;
	}

TInt CMMFMdaAudioToneUtility::Volume()
	{
	return iDevSoundVolume;
	}

void CMMFMdaAudioToneUtility::SetVolume(TInt aVolume) 
	{
	iDevSoundVolume = aVolume;
	}

void CMMFMdaAudioToneUtility::SetPriority(TInt aPriority, TInt aPref)
	{
	iPrioritySettings.iPref = aPref;
	iPrioritySettings.iPriority = aPriority;
	}

void CMMFMdaAudioToneUtility::SetDTMFLengths(TTimeIntervalMicroSeconds32 aToneLength, 
										 TTimeIntervalMicroSeconds32 aToneOffLength,
										 TTimeIntervalMicroSeconds32 aPauseLength)
	{
	}

void CMMFMdaAudioToneUtility::SetRepeats(TInt aRepeatNumberOfTimes, const TTimeIntervalMicroSeconds& aTrailingSilence)
	{
	// iDevSound->SetToneRepeats(aRepeatNumberOfTimes, aTrailingSilence);
	}

void CMMFMdaAudioToneUtility::SetVolumeRamp(const TTimeIntervalMicroSeconds& aRampDuration)
	{
	}

TInt CMMFMdaAudioToneUtility::FixedSequenceCount()
	{
	return 1; // iDevSound->FixedSequenceCount();
	}

_LIT(KFixedSequenceName, "FixedSequenceName");
const TDesC& CMMFMdaAudioToneUtility::FixedSequenceName(TInt aSequenceNumber)
	{
	return KFixedSequenceName;
	}

void CMMFMdaAudioToneUtility::CalculateBalance( TInt& aBalance, TInt aLeft, TInt aRight ) const
	{
	}


void CMMFMdaAudioToneUtility::CalculateLeftRightBalance( TInt& aLeft, TInt& aRight, TInt aBalance ) const
	{
	}


void CMMFMdaAudioToneUtility::SetBalanceL(TInt aBalance) 
	{
	iDevSoundBalance = aBalance;
	}

TInt CMMFMdaAudioToneUtility::GetBalanceL() 
	{
	return iDevSoundBalance; 
	}

void CMMFMdaAudioToneUtility::PrepareToPlayTone(TInt aFrequency, const TTimeIntervalMicroSeconds& aDuration)
	{
	iDuration = aDuration;
	iAsyncCallback->MatoPrepareComplete(KErrNone);
	}

void CMMFMdaAudioToneUtility::PrepareToPlayDualTone(TInt aFrequencyOne, TInt aFrequencyTwo, const TTimeIntervalMicroSeconds& aDuration)
	{
	iDuration = aDuration;
	iAsyncCallback->MatoPrepareComplete(KErrNone);
	}

void CMMFMdaAudioToneUtility::PrepareToPlayDTMFString(const TDesC& aDTMF)
	{
	iDuration = TTimeIntervalMicroSeconds(100);
	iAsyncCallback->MatoPrepareComplete(KErrNone);
	}

void CMMFMdaAudioToneUtility::PrepareToPlayDesSequence(const TDesC8& aSequence)
	{
	iDuration = TTimeIntervalMicroSeconds(100);
	iAsyncCallback->MatoPrepareComplete(KErrNone);
	}

void CMMFMdaAudioToneUtility::PrepareToPlayFileSequence(const TDesC& aFileName)
	{
	iDuration = TTimeIntervalMicroSeconds(100);
	iAsyncCallback->MatoPrepareComplete(KErrNone);
	}
	
void CMMFMdaAudioToneUtility::PrepareToPlayFileSequence(RFile& aFileName)
	{
	iDuration = TTimeIntervalMicroSeconds(100);
	iAsyncCallback->MatoPrepareComplete(KErrNone);
	}




void CMMFMdaAudioToneUtility::PrepareToPlayFixedSequence(TInt aSequenceNumber)
	{
	iDuration = TTimeIntervalMicroSeconds(100);
	iSequenceNumber = aSequenceNumber;
	iAsyncCallback->MatoPrepareComplete(KErrNone);
	}

void CMMFMdaAudioToneUtility::CancelPrepare()
	{
	if (iState == EMdaAudioToneUtilityPrepared)
		{
		iState = EMdaAudioToneUtilityNotReady;
		}
	// Cancel the AO
	iAsyncCallback->Cancel();
	}

TInt CMMFMdaAudioToneUtility::Pause()
	{
	// Handle scenario when Pause is called before playback has started
	if (iState != EMdaAudioToneUtilityPlaying || (iState == EMdaAudioToneUtilityPlaying && !iInitialized))
		{
		return KErrNotReady;
		}

	iState = EMdaAudioToneUtilityPaused;
	return KErrNone;
	}

TInt CMMFMdaAudioToneUtility::Resume()
	{
	if (iState != EMdaAudioToneUtilityPaused)
		{
		return KErrNotReady;
		}

	iState = EMdaAudioToneUtilityPlaying;
	return KErrNone;
	}

void CMMFMdaAudioToneUtility::Play()
	{
	TInt error = KErrNone;

	if ((iState == EMdaAudioToneUtilityPlaying) || (iState == EMdaAudioToneUtilityPaused) || iPlayCalled)
		{
		iState = EMdaAudioToneUtilityNotReady;
		iAsyncCallback->MatoPlayComplete(error);
		return;
		}
			
	iState = EMdaAudioToneUtilityPlaying;

	if (iInitialized)
		{
		// Play() is called after InitializeComplete()
		if (iInitializeState)
			{
			// InitializeComplete() with error other than KErrNone
			iState = EMdaAudioToneUtilityNotReady;
			iAsyncCallback->MatoPlayComplete(iInitializeState);
			}
		else
			{
			PlayAfterInitialized();
			}
		}
	else
		{
		// Play() is called before InitializeComplete()
		iPlayCalled = ETrue;
		}
	}

void CMMFMdaAudioToneUtility::PlayAfterInitialized()
	{
#ifdef _DEBUG
	if (iInitialized == EFalse)
		{
		iPlayCalledBeforeInitialized = ETrue;
		}
#endif
	
	// Really play something!
	// TRAP(error, iDevSound->PlayToneL(c->Frequency(), c->Duration()));
	iTimer->Play(iDuration);
	
#if 0 // the error case 
	iState = EMdaAudioToneUtilityNotReady;
	iAsyncCallback->MatoPlayComplete(error);
	return;
#endif

	if(iPlayStartObserver)
		{
		iAsyncCallback->MatoPlayStarted(KErrNone);
		}
	}
	
void CMMFMdaAudioToneUtility::CancelPlay()
	{
	iTimer->Cancel();
	if(iState == EMdaAudioToneUtilityPlaying || iState == EMdaAudioToneUtilityPaused)
		{
		iState = EMdaAudioToneUtilityPrepared;
		}
	// Cancel the AO
	iAsyncCallback->Cancel();
	iPlayCalled = EFalse;
	}
	

void CMMFMdaAudioToneUtility::SendEventToClient(const TMMFEvent& /*aEvent*/)
	{
	if(iState == EMdaAudioToneUtilityPlaying)
		{
		iState = EMdaAudioToneUtilityPrepared;
		}

	iAsyncCallback->MatoPlayComplete(KErrInUse);
	}


void CMMFMdaAudioToneUtility::RegisterPlayStartCallback(MMdaAudioTonePlayStartObserver& aObserver)
	{
	iPlayStartObserver = &aObserver;
	}

void CMMFMdaAudioToneUtility::MatoPrepareComplete(TInt aError)
	{
	if (!aError)
		{
		iState = EMdaAudioToneUtilityPrepared;
		}
	else 
		{
		iState = EMdaAudioToneUtilityNotReady;
		}

	iCallback.MatoPrepareComplete(aError);
	}

void CMMFMdaAudioToneUtility::MatoPlayComplete(TInt aError)
	{
	iState = EMdaAudioToneUtilityPrepared;
	iCallback.MatoPlayComplete(aError);
	}

void CMMFMdaAudioToneUtility::MatoPlayStarted(TInt aError)
	{
	__ASSERT_DEBUG(aError==KErrNone, Panic(EPlayStartedCalledWithError));
	
	// Not always there is an observer registered
	if(iPlayStartObserver)
		{
		iPlayStartObserver->MatoPlayStarted(aError);
		}
	}

// CustomInferface - just pass on to DevSound. 
TAny* CMMFMdaAudioToneUtility::CustomInterface(TUid aInterfaceId)
	{
	return 0;
	}


CMMFMdaAudioToneObserverCallback* CMMFMdaAudioToneObserverCallback::NewL(MMdaAudioToneObserver& aCallback, MMdaAudioTonePlayStartObserver& aPlayStartCallback)
	{
	return new(ELeave) CMMFMdaAudioToneObserverCallback(aCallback, aPlayStartCallback);
	}

CMMFMdaAudioToneObserverCallback::CMMFMdaAudioToneObserverCallback(MMdaAudioToneObserver& aCallback, MMdaAudioTonePlayStartObserver& aPlayStartCallback) :
	CActive(CActive::EPriorityHigh),
	iCallback(aCallback),
	iPlayStartCallback(aPlayStartCallback)
	{
	CActiveScheduler::Add(this);
	}

CMMFMdaAudioToneObserverCallback::~CMMFMdaAudioToneObserverCallback()
	{
	Cancel();
	}

void CMMFMdaAudioToneObserverCallback::MatoPrepareComplete(TInt aError)
	{
	iAction = EPrepareComplete;
	iErrorCode = aError;

	TRequestStatus* s = &iStatus;
	SetActive();
	User::RequestComplete(s, KErrNone);
	}

void CMMFMdaAudioToneObserverCallback::MatoPlayComplete(TInt aError)
	{
    if(!IsActive())
        {
        iAction = EPlayComplete;
        iErrorCode = aError;
        
        TRequestStatus* s = &iStatus;
        SetActive();
        User::RequestComplete(s, KErrNone);
        }
	}

void CMMFMdaAudioToneObserverCallback::MatoPlayStarted(TInt aError)
	{
	iAction = EPlayStarted;
	iErrorCode = aError;

	TRequestStatus* s = &iStatus;
	SetActive();
	User::RequestComplete(s, KErrNone);
	}

void CMMFMdaAudioToneObserverCallback::RunL()
	{
	switch (iAction)
		{
		case EPrepareComplete:
			{
			iCallback.MatoPrepareComplete(iErrorCode);
			break;
			}
		case EPlayComplete:
			{
			iCallback.MatoPlayComplete(iErrorCode);
			break;
			}
		case EPlayStarted:
			iPlayStartCallback.MatoPlayStarted(iErrorCode);
			break;
		}
	}

void CMMFMdaAudioToneObserverCallback::DoCancel()
	{
	//nothing to cancel
	}

void MMMFClientUtility::ReservedVirtual1() {}
void MMMFClientUtility::ReservedVirtual2() {}
void MMMFClientUtility::ReservedVirtual3() {}
void MMMFClientUtility::ReservedVirtual4() {}
void MMMFClientUtility::ReservedVirtual5() {}
void MMMFClientUtility::ReservedVirtual6() {}

