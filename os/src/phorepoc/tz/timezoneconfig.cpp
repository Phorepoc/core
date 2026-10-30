// timezoneconfig.cpp -- the RCL_3 file + an unreadable time-stamps file is rewritten, not fatal to the server
// Copyright (c) 1997-2009 Nokia Corporation and/or its subsidiary(-ies).
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
// The Phorepoc Authors
//
// Description:
//

#include <bacntf.h>             // CEnvironmentChangeNotifier.
#include <centralrepository.h>  // CRepository.
#include "tzconfigagent.h"
#include "dataprovider.h"
#include <tzupdate.h>   // auto-update notification constants.
#include <vtzrules.h>
#include "tzpersisted.h"
#include "tzlocalizationdb.h"
#include <bautils.h>
#include "cbackuprestorenotification.h"
#ifdef SYMBIAN_ENABLE_SPLIT_HEADERS
#include <tzuserdefineddata.h>
#endif

_LIT8(KUnknownTZName, "Unknown/Zone");

const TUint32 KUnknownTZId = 0x0ffffff0;
const TInt KSecondsPerMinute = 60;

_LIT(KTimeStamps, "c:\\private\\1020383e\\timestamps");
_LIT(KTzDbFileNameFlash,"c:\\private\\1020383e\\TZDB.DBZ");
_LIT(KUserDatabaseName, "c:\\private\\1020383e\\SQLite__tzuserdata.db");
_LIT(KResourceDir, "c:\\resource\\timezonelocalization\\");

// specifies the limits for rules caching.
// (year - KRuleCacheLowerLimit) , (year + KRuleCacheUpperLimit)
// the actual cache includes one more year at year - (KRuleCacheLowerLimit + 1)
// required to provide the offset used at the start of the search 
const TUint KRuleCacheLowerLimit = 2;
const TUint KRuleCacheUpperLimit = 3;

// Security policy for the Publish and Subscribe auto-update notification property.
const TInt KServerUid = 0x1020383E;
const TInt KNextDSTChangePropertyKey(0x10285B32);
_LIT_SECURITY_POLICY_PASS(KReadPolicy);
_LIT_SECURITY_POLICY_S0(KWritePolicy, KServerUid);


CTzConfigAgent::~CTzConfigAgent()
	{
	// Flag that the server is shutting down so that we don't try and set 
	// a new DST Change Timer when the current one is cancelled.
	iShuttingDown = ETrue;
	
	delete iTzDataProvider;

    // Cancel any exitsing timer.
    if (iDstTimer)
        {
        iDstTimer->Cancel();
        delete iDstTimer;
        }
        
	delete iSystemTzCache;
	delete iOtherTzCache;
	delete iRepository;
	delete iChangeNotifier;
	delete iBackupNotifier;
	iTzSwiObserver.RemoveObserver(this);
	iChangeObservers.Close();
	iFs.Close();
	}

/*
*/
CTzConfigAgent::CTzConfigAgent(MTZCfgAgentObserver& aServer, CTzUserDataDb& aTzUserDataDb,
		CTzSwiObserver& aTzSwiObserver) 
	: iTzServer(aServer),
	iEnabled(RTz::ETZAutoDSTUpdateOn),
	iTzSwiObserver(aTzSwiObserver), 
	iShuttingDown(EFalse), 
	iTzUserDataDb(aTzUserDataDb)
	{
	}
/*
*/
void CTzConfigAgent::ConstructL()
	{
	User::LeaveIfError(iFs.Connect());

	// Define a property using Publish and Subscribe.
	// ** Note that the value of this property is unimportant - setting it
	// (to any value) counts as a notification, so we do not set it here.
    TInt err = RProperty::Define(
        NTzUpdate::EUtcOffsetChangeNotification,
        RProperty::EInt,
	    KReadPolicy,
	    KWritePolicy);
	    
	if (err != KErrAlreadyExists)
	    {
	    User::LeaveIfError(err);
	    }
	    
    err = RProperty::Define(
        NTzUpdate::ECurrentTimeZoneId,
        RProperty::EByteArray,
	    KReadPolicy,
	    KWritePolicy);
	    
	if (err != KErrAlreadyExists)
	    {
	    User::LeaveIfError(err);
	    }
 
    err = RProperty::Define(
        NTzUpdate::EHomeTimeZoneId,
        RProperty::EByteArray,
	    KReadPolicy,
	    KWritePolicy);
	    
	if (err != KErrAlreadyExists)
	    {
	    User::LeaveIfError(err);
	    }

	err = RProperty::Define(
        NTzUpdate::ETzRulesChange,
        RProperty::EByteArray,
	    KReadPolicy,
	    KWritePolicy);
	
	if (err != KErrAlreadyExists)
	    {
	    User::LeaveIfError(err);
	    }
 
    err = RProperty::Define(
        NTzUpdate::ETzNamesChange,
        RProperty::EByteArray,
	    KReadPolicy,
	    KWritePolicy);
	    
	if (err != KErrAlreadyExists)
	    {
	    User::LeaveIfError(err);
	    }

	iTzDataProvider = CTzDataProvider::NewL(iFs, iTzSwiObserver, iTzUserDataDb,
		*this);
	
	iDstTimer = CDstEventNotifier::NewL(*this);
		
    iRepository = CRepository::NewL(NTzUpdate::KPropertyCategory);

	// Get the DST auto-update configuration value from the repository.
    User::LeaveIfError(iRepository->Get(EConfigurationKey, iEnabled));

	// Get the current time zone from the repository.
	TInt timeZoneIdentity(0);
    User::LeaveIfError(iRepository->Get(ETimeZoneKey, timeZoneIdentity));

	// Extract the current time zone identifier.  This is stored in the bits
	// not occupied by the old TLocale DST settings.
	TUint localeZone = timeZoneIdentity & ~(EDstHome | EDstEuropean |
		EDstNorthern | EDstSouthern);

	// If the current time zone identifier is set (non-zero) then check that
	// this zone exists. 
    TBool timeZoneExists = EFalse;
    if (localeZone > 0)
    	{
    	timeZoneExists = iTzDataProvider->IsIdInDbL(localeZone);
    	}
    
    CTzId* timeZoneId = NULL;
    if(timeZoneExists)
    	{
    	// Use the current time zone identifier obtained from the repository.
    	timeZoneId = CTzId::NewL(localeZone);
    	}
    else 
		{
		// Either the current time zone is not set in the repository or it is
		// and the time zone does not exist.  In either case we need to use the
		// default time zone identifier.
        timeZoneId = iTzDataProvider->GetDefaultTimeZoneIdL();
        if (!timeZoneId)
            {
            // If we cannot obtain the default time zone identifier then use the
            // "unknown" time zone identifier as a fallback solution.
		    timeZoneId = CTzId::NewL(KUnknownTZId);
            }  
  		}

    CleanupStack::PushL(timeZoneId);

	// Setting the time zone updates the UTC offset and sets a new DST change
	// timer if required.
	SetTimeZoneL(*timeZoneId, static_cast<TAny*>(this), ETrue);

	CleanupStack::PopAndDestroy(timeZoneId);
	
	// Create and start the environment change notifier.
	iChangeNotifier = CEnvironmentChangeNotifier::NewL(CActive::EPriorityIdle, TCallBack(EnvironmentChangeCallBackL, this));
	iChangeNotifier->Start();

	TRAP_IGNORE(PublishNewDstChangeInfoL());

	iTzSwiObserver.AddObserverL(this);

	// Create backup notifier.  Note that file name argument is unimportant.
	iBackupNotifier = CBackupRestoreNotification::NewL(KTzDbFileNameFlash(),*this);

	// Publish any changes that may have happened to TZ databases or resources
	// while the TZ Server was not running.
	PublishTzDbChangeInfoL();
	}

void CTzConfigAgent::PublishTzDbChangeInfoL()
	{
	RFile file;
	CleanupClosePushL(file);

	TTime tzDbModified = Time::NullTTime();
	TTime userDbModified = Time::NullTTime();
	TTime resourceFileModified = Time::NullTTime();

	TInt err = file.Open(iFs, KTimeStamps, EFileRead | EFileWrite);
	if(err == KErrNone || err == KErrNotFound)
		{
		//Read time stamps
		TInt err1 = iFs.Modified(KTzDbFileNameFlash, tzDbModified);
		if(err1 != KErrNone && err1 != KErrNotFound)
			{
			User::Leave(err1);
			}
		err1 = iFs.Modified(KUserDatabaseName, userDbModified);
		if(err1 != KErrNone && err1 != KErrNotFound)
			{
			User::Leave(err1);
			}
		resourceFileModified = GetResourceFileTimeStampsL();
		}
	else
		{
		User::Leave(err);
		}
	
	TBool tzRuleChanged = EFalse;
	TBool tzNameChanged = EFalse;
	
	TTime tzDbSaved = Time::NullTTime();
	TTime userDbSaved  = Time::NullTTime();
	TTime resourceFileSaved  = Time::NullTTime();

	if (err == KErrNone)
		{
		// Phorepoc: a stamps file cut short (a reset between its Replace and its commit) left the server unable to
		// start (KErrEof), and the server is boot-critical: a restart loop on every boot. An unreadable file is
		// treated as an absent one and written again.
		TRAPD(readErr, ReadTimeStampsL(file, tzDbSaved, userDbSaved, resourceFileSaved));
		if (readErr == KErrEof || readErr == KErrCorrupt)
			{
			file.Close();	// Replace below opens it again
			err = KErrNotFound;
			}
		else
			{
			User::LeaveIfError(readErr);
			}
		}
	if (err == KErrNone)
		{
		if(tzDbModified != tzDbSaved)
			{
			tzRuleChanged = ETrue;		
			}
		if(userDbModified != userDbSaved)
			{
			tzRuleChanged = ETrue;
			tzNameChanged = ETrue;
			}
		if(resourceFileModified != resourceFileSaved)
			{
			tzNameChanged = ETrue;		
			}
		}
	else//err == KErrNotFound
		{//Always publish the changes if the time stamps file is not there
		tzRuleChanged = ETrue;
		tzNameChanged = ETrue;
		}
	
	if(tzRuleChanged || tzNameChanged)
		{//Rewrite all three times
		User::LeaveIfError(file.Replace(iFs, KTimeStamps, EFileRead | EFileWrite));
		WriteTimeStampsL(file, tzDbModified, userDbModified, resourceFileModified);
		}
	CleanupStack::PopAndDestroy(&file); // file.Close()
	
	if (tzRuleChanged || tzNameChanged)
		{
		iTzDbHasChanged = ETrue;
		if (tzRuleChanged)
			{
			//Publish the tz rules change
			NTzUpdate::TTzRulesChange rulesChange;
			rulesChange.iUTCTimeOfRulesChange.UniversalTime();
			TPckgBuf<NTzUpdate::TTzRulesChange> bufRules(rulesChange);
			RProperty::Set(NTzUpdate::KPropertyCategory, NTzUpdate::ETzRulesChange, bufRules);
			}
	
		if(tzNameChanged )
			{
			//Publish the names change for user defined DB or resource file change
			NTzUpdate::TTzNamesChange namesChange;
			namesChange.iUTCTimeOfNamesChange.UniversalTime();
			TPckgBuf<NTzUpdate::TTzNamesChange> bufNames(namesChange);
			RProperty::Set(NTzUpdate::KPropertyCategory, NTzUpdate::ETzNamesChange, bufNames);	
			}
		}
	}

void CTzConfigAgent::ReadTimeStampsL(RFile& aFile, TTime& aTzdbSaved, TTime& aUserDbSaved, TTime& aResourceFileSaved)
	{
	RFileReadStream readStream(aFile);
	CleanupClosePushL(readStream);
	
	TUint32 low = readStream.ReadUint32L();
	TUint32 high = readStream.ReadUint32L();
	aTzdbSaved = TTime(MAKE_TINT64(high, low));

	low = readStream.ReadUint32L();
	high = readStream.ReadUint32L();
	aUserDbSaved = TTime(MAKE_TINT64(high, low));

	low = readStream.ReadUint32L();
	high = readStream.ReadUint32L();
	aResourceFileSaved = TTime(MAKE_TINT64(high, low));
	CleanupStack::PopAndDestroy(&readStream);
	}

void CTzConfigAgent::WriteTimeStampsL(RFile& aFile, const TTime& aTzdbModified, const TTime& aUserDbModified, const TTime& aResourceFileModified)
	{
	RFileWriteStream writeStream(aFile);
	CleanupClosePushL(writeStream);
	writeStream << I64LOW(aTzdbModified.Int64());
	writeStream << I64HIGH(aTzdbModified.Int64());
	writeStream << I64LOW(aUserDbModified.Int64());
	writeStream << I64HIGH(aUserDbModified.Int64());
	writeStream << I64LOW(aResourceFileModified.Int64());
	writeStream << I64HIGH(aResourceFileModified.Int64());
	writeStream.CommitL();
	CleanupStack::PopAndDestroy(&writeStream);
	}

/* Get the sum of time stamps of time zone resource files 
 * It returns Time::NullTTime() if there is no resorece files in the "c:\\resource\\timezonelocalization\\" 
 */
TTime  CTzConfigAgent::GetResourceFileTimeStampsL()
	{
	CDir* files = NULL;
	TInt err = iFs.GetDir(KResourceDir, KEntryAttNormal, ESortNone, files);
	CleanupStack::PushL(files);
	TInt64 sum = 0;
	if(err != KErrNone && err != KErrNotFound && err != KErrPathNotFound)
		{
		User::Leave(err);
		}
	if(files)
		{
		TInt count = files->Count();
		for(TInt ii = 0; ii < count; ++ii)
			{
			const TEntry& entry = (*files)[ii];
			sum = sum + entry.iModified.Int64();
			}
		}
	CleanupStack::PopAndDestroy(files);
	if(sum==0)
		{
		return Time::NullTTime();
		}
	return TTime(sum);
	}

void CTzConfigAgent::PublishNewDstChangeInfoL()
	{
    TInt propertyDefineError = RProperty::Define(KUidSystemCategory,
    												KNextDSTChangePropertyKey,
    												RProperty::EByteArray,
    												KReadPolicy,
    												KWritePolicy,
													sizeof(NTzUpdate::TDSTChangeInfo));

	if (propertyDefineError != KErrAlreadyExists)
		{
	    // if there was an actual error defining the property then we should leave
		User::LeaveIfError(propertyDefineError);
		}
	
    // create the next DST change and set some default values
    NTzUpdate::TDSTChangeInfo newDSTChangeInfo;
    newDSTChangeInfo.iVersion = 2;
	
	if ( CalculateNextDstChangeL(newDSTChangeInfo.iNextDSTChangeUTC, newDSTChangeInfo.iPreviousDSTChangeUTC) 
		&& (iEnabled == RTz::ETZAutoDSTUpdateOn) )
		{
		// There is a future DST change in this time zone and auto DST updates are on
		CVTzActualisedRules& actualisedRules = GetTimeZoneRulesL(newDSTChangeInfo.iNextDSTChangeUTC, ETzUtcTimeReference);
		newDSTChangeInfo.iNextUTCOffset = actualisedRules.GetOffsetFromRuleL(newDSTChangeInfo.iNextDSTChangeUTC, ETzUtcTimeReference);
		newDSTChangeInfo.iPreviousUTCOffset = actualisedRules.GetOffsetFromRuleL(newDSTChangeInfo.iPreviousDSTChangeUTC-TTimeIntervalMinutes(1), ETzUtcTimeReference);
		}
	else
		{
		newDSTChangeInfo.iNextDSTChangeUTC = Time::NullTTime();
		newDSTChangeInfo.iNextUTCOffset = 0;
		newDSTChangeInfo.iPreviousDSTChangeUTC = Time::NullTTime();
		newDSTChangeInfo.iPreviousUTCOffset = 0;
		}
	
	// get the current value that is published
	TPckgBuf<NTzUpdate::TDSTChangeInfo> currentNextDSTChangeBuf;
	User::LeaveIfError(RProperty::Get(KUidSystemCategory, KNextDSTChangePropertyKey, currentNextDSTChangeBuf));
	
	// Publish if the next DST change is different from the previous version
	if ((currentNextDSTChangeBuf().iNextUTCOffset != newDSTChangeInfo.iNextUTCOffset)
		|| (currentNextDSTChangeBuf().iNextDSTChangeUTC != newDSTChangeInfo.iNextDSTChangeUTC)
		|| (currentNextDSTChangeBuf().iPreviousUTCOffset != newDSTChangeInfo.iPreviousUTCOffset)
		|| (currentNextDSTChangeBuf().iPreviousDSTChangeUTC != newDSTChangeInfo.iPreviousDSTChangeUTC)
		|| (propertyDefineError != KErrAlreadyExists))
		{
		// package the data and set the property value 
		TPckgBuf<NTzUpdate::TDSTChangeInfo> newDSTChangeInfoBuf(newDSTChangeInfo);
	    User::LeaveIfError(RProperty::Set(KUidSystemCategory, KNextDSTChangePropertyKey, newDSTChangeInfoBuf));
		}
	}

/*
*/
CTzConfigAgent* CTzConfigAgent::NewL(MTZCfgAgentObserver& aServer, CTzUserDataDb& aUserDataDb,
		CTzSwiObserver& aTzSwiObserver)
	{
	CTzConfigAgent* self = new(ELeave) CTzConfigAgent(aServer, aUserDataDb, aTzSwiObserver);
	CleanupStack::PushL(self);
	self->ConstructL();
	CleanupStack::Pop(self);		
	return self;
	}

const CTzId& CTzConfigAgent::GetTimeZoneIdL() const
	{
	if (!iSystemTzCache)
		{
		User::Leave(KErrNotFound);
		}

	return iSystemTzCache->TimeZoneId();
	}

const CTzId& CTzConfigAgent::SetTimeZoneL(const CTzId& aZoneId, const TAny* aRequester, TBool aUpdateCentralRepository, TBool aForceCacheUpdate)
	{
	TInt oldTzId = 0;
	TBool zoneChanged = EFalse;
	TTime now;
	now.UniversalTime();

	if (!iSystemTzCache)
		{
		iSystemTzCache = CSystemTzRulesCache::NewL(aZoneId, *this, now);
		}
	else if (iSystemTzCache->TimeZoneId() == aZoneId && !aForceCacheUpdate)
		{
		return (iSystemTzCache->TimeZoneId());
		}
	else
		{
		oldTzId = iSystemTzCache->TimeZoneId().TimeZoneNumericID();
		// replace the rule. create new before deleting old
		CSystemTzRulesCache* newZone = CSystemTzRulesCache::NewL(aZoneId, *this, now);
		CleanupStack::PushL(newZone);
		
		//For Invalid Timezone, rules doesnot exist 
		CTzRules& newZoneRules =  newZone->GetEncodedTimeZoneRulesL();
		if (newZoneRules.Count() == 0)
		    {
		    User::Leave(KErrNotFound);
		    }

		delete iSystemTzCache;
		iSystemTzCache = newZone;

		CleanupStack::Pop(newZone);
		zoneChanged = ETrue;
		}

	TInt offset = iSystemTzCache->Rules().GetOffsetFromRuleL(now, ETzUtcTimeReference);
		
	// We need to publish an automatic time update event 
	// Stop the auto time update observer and
	// remove the published auto-update time.
	// if the old and new timezone ids are the same but
	// the new offset differs from what was originally there

	if (iSystemTzCache->TimeZoneId().TimeZoneNumericID() != KUnknownTZId)
		{
		// Now set the new time zone and new UTC Offset.
		TInt TimeZoneIdentity((TDaylightSavingZone)(iSystemTzCache->TimeZoneId().TimeZoneNumericID()));
		
 		if(aUpdateCentralRepository)
			{
			// Set the currently used time zone from central repository
			User::LeaveIfError(iRepository->Set(ETimeZoneKey, TimeZoneIdentity));	
			}
            
        // Set the new UTC Offset.
		const TTimeIntervalSeconds newUtcOffset = offset * KSecondsPerMinute;
		User::SetUTCOffset(newUtcOffset);

		// replace cached current time zone
		if (zoneChanged)
			{
			iTzServer.NotifyTZStatusChange(RTz::ETZSystemTimeZoneChanged, aRequester);
			}

// Do not publish updates or set timers if compiled as part of
// the unit test as this will conflict with the Time Zone Server.
#ifndef SYMBIAN_TZ_UNIT_TEST

        // Publish UTC Offset and set DST Change Timer if configured.
        // Note that it would make sense to always publish the UTC Offset here (if it
        // changes) as this is not a change due to the auto-update functionality, but 
        // this would make the PREQ 234 changes only partly configurable.
        if (iEnabled == RTz::ETZAutoDSTUpdateOn)
            {
            // Set a new DST Change Timer if required.
            SetDstChangeTimerL();
            }
#endif  // SYMBIAN_TZ_UNIT_TEST

		NTzUpdate::TTimeZoneChange change;
		change.iNewTimeZoneId = iSystemTzCache->TimeZoneId().TimeZoneNumericID();
		change.iOldTimeZoneId = oldTzId;
		
		TPckgBuf<NTzUpdate::TTimeZoneChange> changeBuf(change);
		RProperty::Set(NTzUpdate::KPropertyCategory, NTzUpdate::ECurrentTimeZoneId, changeBuf);
		}
	
	iTzDataProvider->ReleaseTzRules();
	
	TRAP_IGNORE(PublishNewDstChangeInfoL());
	
	return (iSystemTzCache->TimeZoneId());
	}

CTzRules& CTzConfigAgent::GetEncodedTimeZoneRulesL()
	{
	return iSystemTzCache->GetEncodedTimeZoneRulesL();
	}

TInt CTzConfigAgent::GetEncodedTimeZoneRulesSizeL(const TTime& aStartTime, const TTime& aEndTime, TTzTimeReference aTimeRef)
	{
	TInt size = iSystemTzCache->GetEncodedTimeZoneRulesSizeL(aStartTime, aEndTime, aTimeRef);
	iTzDataProvider->ReleaseTzRules();
	return size;
	}

CTzRules& CTzConfigAgent::GetForeignEncodedTimeZoneRulesL()
	{
	if (iOtherTzCache == NULL)
		{
		User::Leave(KErrNotFound);
		}
		
	return iOtherTzCache->GetEncodedTimeZoneRulesL();
	}
		
TInt CTzConfigAgent::GetForeignEncodedTimeZoneRulesSizeL(const CTzId& aZoneId, const TTime& aStartTime, const TTime& aEndTime, TTzTimeReference aTimeRef)
	{
	if (!iOtherTzCache)
		{
		iOtherTzCache = CTZRulesCache::NewL(aZoneId, *this, aStartTime);
		}
	else if (iOtherTzCache->TimeZoneId() != aZoneId)
		{
		// ids are different.
		// replace the rule. create new before deleting old
		CTZRulesCache* newZone = CTZRulesCache::NewL(aZoneId, *this, aStartTime);

		delete iOtherTzCache;
		iOtherTzCache = newZone;
		}

	TInt size = iOtherTzCache->GetEncodedTimeZoneRulesSizeL(aStartTime, aEndTime, aTimeRef);
	iTzDataProvider->ReleaseTzRules();
	return size; 
	}
										
#if defined(_DEBUG)
/**
Required for OOM testing.  To obtain a balanced heap cell count check we need to
reset any cached foreign time zone rules.
*/
void CTzConfigAgent::ResetForeignTimeZoneRulesCache(void)
	{
	delete iOtherTzCache;
	iOtherTzCache = NULL;
	}		
#endif
	
CVTzActualisedRules& CTzConfigAgent::GetTimeZoneRulesL(const TTime& aTime,TTzTimeReference aTimerRef)
	{
	CVTzActualisedRules& rules = iSystemTzCache->GetTimeZoneRulesL(aTime,aTimerRef);
	iTzDataProvider->ReleaseTzRules();
	return rules; 
	}	
	
CVTzActualisedRules& CTzConfigAgent::GetForeignTimeZoneRulesL(const CTzId& aZoneId,
															  const TTime& aTime,
															  TTzTimeReference aTimerRef)
	{
	if (!iOtherTzCache)
		{
		iOtherTzCache = CTZRulesCache::NewL(aZoneId, *this, aTime);
		}
	else if (iOtherTzCache->TimeZoneId() != aZoneId)
		{
		// ids are different.
		// replace the rule. create new before deleting old
		CTZRulesCache* newZone = CTZRulesCache::NewL(aZoneId, *this, aTime);

		delete iOtherTzCache;
		iOtherTzCache = newZone;
		}

	CVTzActualisedRules& rules = iOtherTzCache->GetTimeZoneRulesL(aTime,aTimerRef);
	iTzDataProvider->ReleaseTzRules();
	return rules; 
	}	

/*
Retrieves the UTC offset for an array of numeric time zone ids.
The offset is written back into aTimeZoneIdArray at the position that the numeric id 
was read from, overwriting the id.
@param aTimeZoneIdArray flat buffer consisting of a TInt for the number of ids,
followed by the numeric time zone id (TInt) for which the current UTC offset is required.
*/
void CTzConfigAgent::GetOffsetsForTimeZoneIdsL(CBufFlat& aTimeZoneIdArray)
	{
	iTzDataProvider->GetOffsetsForTimeZoneIdsL(aTimeZoneIdArray);
	iTzDataProvider->ReleaseTzRules();
	}

void CTzConfigAgent::ConvertL(const CTzId& aZone, TTime& aTime, TTzTimeReference aTimerRef)
	{
	CVTzActualisedRules& rules = GetForeignTimeZoneRulesL(aZone, aTime, aTimerRef);
	CTzRules& tzRules = GetForeignEncodedTimeZoneRulesL();
	iTzDataProvider->ReleaseTzRules();
	
	tzRules.ConvertTime(rules, aTime, aTimerRef);
	}

void CTzConfigAgent::ConvertL(TTime& aTime, TTzTimeReference aTimerRef)
	{
    CVTzActualisedRules& rules = iSystemTzCache->GetTimeZoneRulesL(aTime,aTimerRef);
	CTzRules& tzRules = GetEncodedTimeZoneRulesL();
	iTzDataProvider->ReleaseTzRules();
	tzRules.ConvertTime(rules, aTime, aTimerRef);
	}

TInt CTzConfigAgent::IsDaylightSavingOnL(const CTzId& aZone, TTime& aUTCTime)
{
	const CVTzActualisedRules& rules = GetForeignTimeZoneRulesL(aZone, aUTCTime, ETzUtcTimeReference);
	iTzDataProvider->ReleaseTzRules();
	return rules.IsDaylightSavingOn(aUTCTime);
}

// There is a change to the TZ database. A new one may have been installed
// Reset the current time zone information to take advantage of any changes.
void CTzConfigAgent::NotifyTZDataStatusChangeL(RTz::TTzChanges aChange)
	{
	if (aChange == RTz::ETZLocalizationDataChanged)
		{
		TInt j = 0;
  		TInt jEnd = iChangeObservers.Count();
  		while (j < jEnd)
  			{
  			iChangeObservers[j]->NotifyTZDataStatusChangeL(aChange);
  			++j;
  			}

		NTzUpdate::TTzNamesChange namesChange;
		namesChange.iUTCTimeOfNamesChange.UniversalTime();
		TPckgBuf<NTzUpdate::TTzNamesChange> bufNames(namesChange);
		User::LeaveIfError(RProperty::Set(NTzUpdate::KPropertyCategory, NTzUpdate::ETzNamesChange, bufNames));
		}
	else 
		{
		delete iOtherTzCache;
		iOtherTzCache = NULL;
		SetTimeZoneL(iSystemTzCache->TimeZoneId(), static_cast<TAny*>(this), ETrue, ETrue);
		iTzDataProvider->ReleaseTzRules();

		// notify change
		iTzServer.NotifyTZStatusChange(aChange, static_cast<TAny*>(this));
		
		//Publish the property
		NTzUpdate::TTzRulesChange rulesChange;
		rulesChange.iUTCTimeOfRulesChange.UniversalTime();
		TPckgBuf<NTzUpdate::TTzRulesChange> bufRules(rulesChange);
		User::LeaveIfError(RProperty::Set(NTzUpdate::KPropertyCategory, NTzUpdate::ETzRulesChange, bufRules));
		}
	}

/**
Calculates the time/date (in UTC) of the next DST change event that will take
place for the current time zone. Note that there may not be a DST change for
the current time zone, in which case EFalse is returned and aNextDstEvent is
not changed.

@param aNextDstEvent returns the time of the next DST change event.
@return ETrue if a change event was found, else EFalse.
@internalComponent

*/
TBool CTzConfigAgent::CalculateNextDstChangeL(TTime& aNextDstEvent)
	{
	TTime dummy;
	return CalculateNextDstChangeL(aNextDstEvent, dummy);
	}

/**
Calculates the time/date (in UTC) of the next DST change event that will take
place for the current time zone. Note that there may not be a DST change for
the current time zone, in which case EFalse is returned and aNextDstEvent is
not changed.

@param aNextDstEvent returns the time of the next DST change event.
@return ETrue if a change event was found, else EFalse.
@internalComponent

*/
TBool CTzConfigAgent::CalculateNextDstChangeL(TTime& aNextDstEvent, TTime& aPreviousDstEvent)
    {
    // Get the current time/date.
    TTime now;
    now.UniversalTime();
    
    // Get the actualised rules for the current time zone
    // - the next DST change will always exist within the range returned.
    // (Note that the current time zone may not have any DST changes.)
	CVTzActualisedRules& rules = GetTimeZoneRulesL(now,ETzUtcTimeReference);
		
    // Iterate through the actualised rules until we get the next rule change 
    // after the current time/date. (Or we reach the end of the actualised rules 
    // - in which case no DST change applies.)
    const TInt KRuleCount = rules.Count();
    
    // Store the previous offset for converting the time of change to utc.
    // Note: for times before any rules applied, the dst offset is always 0.
    TInt offset;
    
    aPreviousDstEvent = Time::MinTTime();
    for (TInt i = 0; i < KRuleCount; i++)
        {
        // Get the next time of change as a UTC time.
        TTime utcTimeOfChange = rules[i].iTimeOfChange;
     
        __ASSERT_ALWAYS(rules[i].iTimeReference!=ETzStdTimeReference, RTz::Panic(RTz::EPanicUnsupportedTimeReference));
	
        if (rules[i].iTimeReference == ETzWallTimeReference)
            {
        	// Get the previous offset as this was used to create the
        	// local time version of utcTimeOfChange.
    		if (i > 0)
    			{
    			// previous iNewOffset contains the STD offset plus
    			// the previous DST offset
    			offset = rules[i-1].iNewOffset;
    			}
    		else
	    		{
	    		// First offset will contain just the STD offset
	    		offset = rules[i].iNewOffset;	
	    		}
            utcTimeOfChange -= static_cast<TTimeIntervalMinutes>(offset);
            }
        
        if (utcTimeOfChange <= now)
        	{
        	aPreviousDstEvent = utcTimeOfChange;
        	}
        else
            {
            aNextDstEvent = utcTimeOfChange;
            return ETrue;
            }
        }
        
    // No DST change has been found.
    return EFalse;
    }


/**
Calculates the current UTC offset using time zone rules for the current time
zone. Note that this may be different from the value set in User::UTCOffset.

@param aOffset returns the current UTC Offset in seconds
@internalComponent

*/
void CTzConfigAgent::CalculateUtcOffsetL(TTimeIntervalSeconds& aOffset)
    {
    // Get the current time/date.
    TTime now;
    now.UniversalTime();
    
    // Get actualised rules for the current time zone.
	CVTzActualisedRules& rules = GetTimeZoneRulesL(now,ETzUtcTimeReference);

    // Get the current UTC Offset in minutes.
    TInt offsetInMinutes = rules.GetOffsetFromRuleL(now, ETzUtcTimeReference);
      
    // Multiply up to get the offset in seconds.
    aOffset = offsetInMinutes * KSecondsPerMinute;
    }
    
    

/**
Gets the time/date (in UTC) of the next DST change event for the current time
zone and sets a timer if it is a valid time.

@internalComponent

*/
void CTzConfigAgent::SetDstChangeTimerL()
    {
    // Cancel any existing DST Change Timer.
    iDstTimer->Cancel();

    // Get the time of the next DST change event (in UTC).
    TTime nextTimer;
    
    if (CalculateNextDstChangeL(nextTimer))
        {
        // If the time is valid then set the timer.
        iDstTimer->SetTimer(nextTimer);
        }
    }

/**
Handles cancellation or completion of a previous DST Change Timer. Updates the
UTC Offset and sets a new DST Change Timer as necessary.

@internalComponent

*/
void CTzConfigAgent::HandleDstChangeTimerL()
    {
	// UTC offset must be updated only when Auto Update is ON
	if (iEnabled == RTz::ETZAutoDSTUpdateOn)
		{
		UpdateUtcOffsetL();
		}

	// Notification must be generated when Auto Update is ON or NOTIFY
	// (unless the server is shutting down)
	if (iEnabled != RTz::ETZAutoDSTUpdateOff && !iShuttingDown)
 	    {
		// reset dst timer
		SetDstChangeTimerL();

		// Publish update notification.
		// Note that the value published is the same as
		// the AutoUpdate Configuration
		TInt err = RProperty::Set(
   				NTzUpdate::KPropertyCategory, 
   				NTzUpdate::EUtcOffsetChangeNotification, iEnabled);
			User::LeaveIfError(err);
	    }
	
	TRAP_IGNORE(PublishNewDstChangeInfoL());
    }

    
/**
Calculates what the UTC Offset should be - if this differs from the current
offset then the new offset is set using User::SetUTCOffset() and subscribed
clients are notified of the change through the Publish and Subscribe API.

@internalComponent

*/
void CTzConfigAgent::UpdateUtcOffsetL()
    {
    
    // Calculate the current UTC Offset in seconds.
    TTimeIntervalSeconds currentOffset;
    CalculateUtcOffsetL(currentOffset);
    
    if (currentOffset != User::UTCOffset())
		{
    	// Set the new UTC Offset.
		iChangeNotifier->Cancel();
    	User::SetUTCOffset(currentOffset);
		// enable observer again
		iChangeNotifier->Start();
    	// Notify session observer.
		iTzServer.NotifyTZStatusChange(RTz::ETZAutomaticTimeUpdate, static_cast<TAny*>(this));
		}
    }
        
/**
Sets the configuration of the UTC Offset auto-update functionality.

@param aUpdateEnabled If set to RTz::ETZAutoDSTUpdateOn then the UTC Offset is automatically
updated for changes to Daylight Savings Time. If set to RTz::ETZAutoDSTUpdateOff then auto-update
is disabled. The RTz::ETZAutoDSTNotificationOnly - Means that the client app needs to confirm that the 
time should be updated whenever a DST event occurs.


@internalComponent

*/
void CTzConfigAgent::SetAutoUpdateBehaviorL(TInt aUpdateEnabled)
    {
    // Check if the configuration has changed.
    if (aUpdateEnabled != iEnabled)
        {
        // Persist the new configuration.
        iEnabled = aUpdateEnabled;
       	iRepository->Set(EConfigurationKey, iEnabled);

        // Handle configuration change.
        if (aUpdateEnabled != RTz::ETZAutoDSTUpdateOff)
            {
			// Update the UTC Offset (if required) and set a new DST Change Timer.       

            SetDstChangeTimerL();
			// Don't update Kernel for Notify - only for ON
			if (aUpdateEnabled == RTz::ETZAutoDSTUpdateOn)
				{
				UpdateUtcOffsetL();   
				}
            }
        else
            {
            // Cancel any existing DST Change Timer.
            iDstTimer->Cancel();
            }
        }
    
    TRAP_IGNORE(PublishNewDstChangeInfoL());
    }

 /**
Change current local time.

@param aHomeTime The time to set in wall-clock time.

@return An error code. KErrNone is expected unless there is an error in
converting the given local time to UTC.

@internalComponent

*/
TInt CTzConfigAgent::SetHomeTimeL(const TTime& aHomeTime)     
	{
	TTime utcTime(aHomeTime);

	ConvertL(utcTime, ETzWallTimeReference);

	//Cancel environment change notification in case double updating UTC offset.
	iChangeNotifier->Cancel();   

	//Cancel DST timer.
	iDstTimer->Cancel();

	TTimeIntervalSeconds utcOffset(0);
	aHomeTime.SecondsFrom(utcTime,utcOffset);

	//Update kernel time and utc offset.
	TInt result = User::SetUTCTimeAndOffset(utcTime, utcOffset);

	// Get the time of the next DST change event (in UTC).
	TTime nextTimer;

	if (iEnabled != RTz::ETZAutoDSTUpdateOff)
		{
		if (CalculateNextDstChangeL(nextTimer))
			{
			// Restart the dst timer.
			iDstTimer->SetTimer(nextTimer);
			}
		}

	//Restart observe environment change notification.
	iChangeNotifier->Start();
	
	TRAP_IGNORE(PublishNewDstChangeInfoL());

	return result;
	}

TInt CTzConfigAgent::SetUnknownTimeZoneTimeL(const TTime& aUTCTime, const TInt aUTCOffset, TBool aPersistInCenRep)     
	{
	//
	// Cancel DST timer. (It will not be restarted, as we won't have DST rules for this unknown zone)
	//
	iDstTimer->Cancel();

	//Update kernel time and utc offset.
	const TTimeIntervalSeconds newUtcOffset = aUTCOffset * KSecondsPerMinute;

	//
	// Cancel environment change notification in case double updating UTC offset.
	//
	iChangeNotifier->Cancel();   

	TInt result = User::SetUTCTimeAndOffset(aUTCTime, newUtcOffset);
	//
	// Restart environment change notification.
	//
	iChangeNotifier->Start();

	User::LeaveIfError(result);

	TInt oldTzId = iSystemTzCache->TimeZoneId().TimeZoneNumericID();

	// replace the rule, create an new unknown before deleting old
	CTzId* timeZoneId = CTzId::NewL(KUnknownTZId);
	CleanupStack::PushL(timeZoneId);

	// CSystemTzRulesCache::NewL will create a default rule based on the offset in the Kernel
	CSystemTzRulesCache* newZone = CSystemTzRulesCache::NewL(*timeZoneId, *this, aUTCTime);
	CleanupStack::PushL(newZone);

	delete iSystemTzCache;
	iSystemTzCache = newZone;

	if (aPersistInCenRep)
		{
		// Set the unknown time zone in central repository
		// 0 is used to represent the Unknow time zone value although in the code KUnknownTZId is used
		User::LeaveIfError(iRepository->Set(ETimeZoneKey, (TInt)KUnknownTZId));
		}
	else
		{
		// Set the unknown time zone in central repository
		// 0 is used to represent the Unknow time zone value although in the code KUnknownTZId is used
		User::LeaveIfError(iRepository->Set(ETimeZoneKey, 0));
		}

	CleanupStack::Pop(newZone);
	CleanupStack::PopAndDestroy(timeZoneId);

	iTzServer.NotifyTZStatusChange(RTz::ETZSystemTimeZoneChanged, static_cast<TAny*>(this));


	NTzUpdate::TTimeZoneChange change;
	change.iNewTimeZoneId = 0;
	change.iOldTimeZoneId = oldTzId;

	TPckgBuf<NTzUpdate::TTimeZoneChange> changeBuf(change);
	RProperty::Set(NTzUpdate::KPropertyCategory, NTzUpdate::ECurrentTimeZoneId, changeBuf);

	iTzDataProvider->ReleaseTzRules();

	return KErrNone;
	}

/**
Gets the daylight saving update
*/
TInt CTzConfigAgent::AutoUpdateSetting()
    {
	return iEnabled;
    }


/**
Observer method called as a result of a change in system time. Updates the UTC
Offset and sets a new DST Change Timer as necessary.

@internalComponent

*/
void CTzConfigAgent::HandleSystemTimeChangeL()
    {
    HandleDstChangeTimerL();
    }

/**
Observer method called as a result of a DST Change Timer completing. Updates
the UTC Offset and sets a new DST Change Timer as necessary.

@internalComponent

*/
void CTzConfigAgent::HandleDstTimerCompleteL()
    {
    HandleDstChangeTimerL();
    }

/**
Observer method called as a result of a DST Change Timer being cancelled. (The
underlying RTimer is cancelled - for example, because of a change to the UTC
offset.) This is not called when Cancel() or DoCancel() are called.) Updates
the UTC Offset and sets a new DST Change Timer as necessary.

@internalComponent

*/
void CTzConfigAgent::HandleDstTimerCancellationL()
    {
	HandleDstChangeTimerL();
    }
    
/**
Observer method called due to an error with the DST Change Timer that tries to
recover from the error. We only expect this to get called with a value of
KErrUnderflow (if the system time is changed to a time past the current timer)
or a value of KErrOverflow as returned by RTimer.

@param aError The error returned from the timer.
@internalComponent

*/
void CTzConfigAgent::DstTimerErrorOccurredL(TInt aError)
    {
    // Remove the compiler warning about aError not being used. (It's useful in debugging.)
    aError = aError;
    
    // Try and recover by updating the UTC Offset and setting a new 
    // DST Change Timer as necessary.
    HandleDstChangeTimerL();
    }

/**
Environment observer callback - called when the kernel causes a system change.
This can be used to pick up any changes when there is no DST Change Timer and
may also provide a faster response to system time or locale changes.

@param aSelf A pointer to this class.
@return Always returns ETrue.
@internalComponent

*/
TBool CTzConfigAgent::EnvironmentChangeCallBackL(TAny* aSelf)
	{
	CTzConfigAgent* self = static_cast<CTzConfigAgent*>(aSelf);

	const TInt KChanges = self->iChangeNotifier->Change();
	
	if (KChanges == KErrCancel)
	    {
	    return ETrue;   // Observer is being cancelled.
	    }

    if (KChanges & (EChangesLocale | EChangesSystemTime))
		{
        // Update the UTC Offset and set a new DST Change Timer as necessary.
        self->UpdateUtcOffsetL();
        self->SetDstChangeTimerL();
		}
	    
	return ETrue;
	}
	
/** Reponding to the change in user defined time zone
 
The CTzConfigAgent class maintains a cache of TZ rules (including actualised rules for 
the last used rule) for the system (also known as current) TZ in the iSystemTzCache member
and for a “foreign” TZ in the iOtherTzCache member.  Either of these members could be a cache
of the TZ rules from a user-defined TZ.  If the associated user-defined TZ data is updated
or deleted or a restore of user-defined TZ data has taken place then this cache needs to be
updated appropriately.
*/
void CTzConfigAgent::NotifyUserTzRulesChange(TTzUserDataChange aChange)
	{
	TRAP_IGNORE(NotifyUserTzRulesChangeL(aChange));
	}
void CTzConfigAgent::NotifyUserTzRulesChangeL(TTzUserDataChange aChange)
	{
	//Only interested when a user TZ rule is updated or deleted
	if (aChange.iOperation == ETzUserDataUpdated ||	aChange.iOperation == ETzUserDataDeleted)
		{	
		//Chached time zone (iOtherTzCache) should be cleaned off.
		delete iOtherTzCache;
		iOtherTzCache = NULL;
		
		TUint id = aChange.iTzId;
		if(iSystemTzCache && id == iSystemTzCache->TimeZoneId().TimeZoneNumericID())	
			{
			CTzId* tzid = NULL;
			if (aChange.iOperation == ETzUserDataUpdated)
				{
				//Cached symstem time zone (iSystemTzCache) should be updated.
				tzid = CTzId::NewL(id);
				}
			else if (aChange.iOperation == ETzUserDataDeleted)
				{//Cached symstem time zone (iSystemTzCache) should be reverted to the default time zone.
				tzid = iTzDataProvider->GetDefaultTimeZoneIdL();
				if(!tzid)
					{
					tzid = CTzId::NewL(KUnknownTZId);
					}
				}
			CleanupStack::PushL(tzid);
			SetTimeZoneL(*tzid, static_cast<TAny*>(this), ETrue, ETrue);	
			CleanupStack::PopAndDestroy(tzid);	
			}
		}
	}
 
void CTzConfigAgent::NotifyUserTzNamesChange(TTzUserDataChange /*aChange*/)
    {// There is no action needed when the names of a user defined time zone has been changed.
    }
  
void CTzConfigAgent::AddObserverL(MTzDataObserver* aChangeObs)
	{
	User::LeaveIfError(iChangeObservers.Append(aChangeObs));
	}

void CTzConfigAgent::RemoveObserver(MTzDataObserver* aChangeObs)
	{
	TInt j = 0;
	TInt jEnd = iChangeObservers.Count();
	while (j < jEnd)
		{
		if (iChangeObservers[j] == aChangeObs)
			{
			iChangeObservers.Remove(j);
			break;
			}
		++j;
		}
	}


void CTzConfigAgent::BackupBeginningL()
	{
	iTzLocalizationDb->BackupBeginningL();
	iTzDataProvider->BackupBeginningL();
	}

void CTzConfigAgent::BackupCompletedL()
	{
	iTzDataProvider->BackupCompletedL();
	iTzLocalizationDb->BackupCompletedL();
	}
void CTzConfigAgent::RestoreBeginningL()
	{
	iTzLocalizationDb->RestoreBeginningL();
	iTzDataProvider->RestoreBeginningL();
	}

void CTzConfigAgent::RestoreCompletedL(TInt aRestoreResult)
	{
	if(aRestoreResult == KErrNone)
		{
		//Both read only DB and user defined DB will refresh the data
		iTzDataProvider->RestoreCompletedL();
		
		//Update cached tz rules
		delete iOtherTzCache;
 		iOtherTzCache = NULL;

		if(iSystemTzCache)
			{
			// Find out whether the cached system time zone rule still exists in
			// the rules database.  If not, set the current time zone using the
			// default time zone identifier.
			TBool timeZoneExists = iTzDataProvider->IsIdInDbL(iSystemTzCache->TimeZoneId().TimeZoneNumericID());
			if(timeZoneExists)
				{
				SetTimeZoneL(iSystemTzCache->TimeZoneId(), static_cast<TAny*>(this), EFalse, ETrue);
				}
			else
				{
				CTzId* tzid = iTzDataProvider->GetDefaultTimeZoneIdL();
				CleanupStack::PushL(tzid);
				SetTimeZoneL(*tzid, static_cast<TAny*>(this), EFalse, ETrue);
				CleanupStack::PopAndDestroy(tzid);
				}
			}
		
		// Notify change to the clients.
		iTzServer.NotifyTZStatusChange(RTz::ETZDatabaseChanged, static_cast<TAny*>(this));

		// Notify the localization DB.
		iTzLocalizationDb->RestoreCompletedL();
		
		//Publish tz rules change 
		NTzUpdate::TTzRulesChange rulesChange;
		rulesChange.iUTCTimeOfRulesChange.UniversalTime();
		TPckgBuf<NTzUpdate::TTzRulesChange> bufRules(rulesChange);
		RProperty::Set(NTzUpdate::KPropertyCategory, NTzUpdate::ETzRulesChange, bufRules);
		
		//Publish the names change for user defined DB
		NTzUpdate::TTzNamesChange namesChange;
		namesChange.iUTCTimeOfNamesChange.UniversalTime();
		TPckgBuf<NTzUpdate::TTzNamesChange> bufNames(namesChange);
		RProperty::Set(NTzUpdate::KPropertyCategory, NTzUpdate::ETzNamesChange, bufNames);	
		}
	}
//------------------------------------------------------------------------------------
CTZRulesCache::~CTZRulesCache()
	{
	delete iTimeZoneId;		// time zone Id
	delete iActualisedRules;// caches the last used rule for zone
	delete iEncodedRules;
	}

// template method used for final construction of a time zone rules cache
// this is the base implementation -- see ConstructL()
void CTZRulesCache::SetDefaultZoneIdL()
	{
	// does nothing
	User::Leave(KErrNotFound);
	}

const CTzRules& CTZRulesCache::doGetEncodedTimeZoneRulesL(TInt aStartYear, TInt aEndYear)
	{
	CTzDataProvider& tzDataProvider = iTimeZoneConfigAgent.TzDataProvider();
	
	CTzRules* newRules = CTzRules::NewL(aStartYear, aEndYear);
	CleanupStack::PushL(newRules);
	
	tzDataProvider.GetTzRulesL(*newRules, *iTimeZoneId);


	CleanupStack::Pop(newRules);
	
	// replace current cached rules
	delete iEncodedRules;
	iEncodedRules = newRules;
	
	return *iEncodedRules;
	}
	
void CTZRulesCache::doGetActualisedTimeZoneRulesL(const TTime& aTime)
	{
	TDateTime dateTime(aTime.DateTime() );

	// At least we need the rule for the year preceeding the converted time
	// in order to convert the time to use it to find the correct rule. (Sounds roundabout!)
	// hence the extra one added to the cache limit

	// avoid over/under flow
	TUint startYear = dateTime.Year();
	TUint endYear = startYear;
	if (startYear >= (KRuleCacheLowerLimit+1))
		{
		startYear -= (KRuleCacheLowerLimit+1);
		}
	if (endYear < (KMaxTUint - KRuleCacheUpperLimit))
		{
		endYear += KRuleCacheUpperLimit;
		}

	CVTzActualisedRules* newRules = CVTzActualisedRules::NewL(startYear, endYear);
	CleanupStack::PushL(newRules);

    iEncodedRules->GetActualisedRulesL(*newRules);

	if (newRules->Count() == 0) // there will always be at least one rule
		{
		User::Leave(KErrNotFound);
		}

	CleanupStack::Pop(newRules);

	// replace current cached rules
	delete iActualisedRules;
	iActualisedRules = newRules;
	}
	
void CTZRulesCache::ConstructL(const CTzId& aTimeZoneId, const TTime& aTime)
	{
	if (aTimeZoneId.TimeZoneNumericID() != KUnknownTZId)
		{
		iTimeZoneId	= aTimeZoneId.CloneL();

		TRAPD(err,doGetEncodedTimeZoneRulesL(KMinYear, KMaxYear));
		if (err == KErrNotFound)
			{
			SetDefaultZoneIdL();	
			}
		else
			{
			User::LeaveIfError(err);
	
			// avoid over/under flow
			TDateTime dateTime(aTime.DateTime() );
			TUint startYear = dateTime.Year();
			TUint endYear = startYear;
			
			if (startYear >= (KRuleCacheLowerLimit+1))
				{
				startYear -= (KRuleCacheLowerLimit+1);
				}
			if (endYear < (KMaxTUint - KRuleCacheUpperLimit))
				{
				endYear += KRuleCacheUpperLimit;
				}
		
			iActualisedRules = CVTzActualisedRules::NewL(startYear, endYear);
			iEncodedRules->GetActualisedRulesL(*iActualisedRules);
			}
		}
	else
		{
		// Call Template Method to Set defaultZoneId
		SetDefaultZoneIdL();
		}
	}


CTZRulesCache* CTZRulesCache::NewL(const CTzId& aTimeZoneId,
								CTzConfigAgent& aTimeZoneConfigAgent,
								const TTime& aTime)
	{
	CTZRulesCache* self = new (ELeave) CTZRulesCache(aTimeZoneConfigAgent);
	CleanupStack::PushL(self);

	self->ConstructL(aTimeZoneId, aTime);

	CleanupStack::Pop(self);
	return self;
	}

CTzRules& CTZRulesCache::GetEncodedTimeZoneRulesL()
	{
	// return the rules held in cache
	if (iEncodedRules == NULL)
		{
		User::Leave(KErrNotFound);
		}
	return *iEncodedRules;
	}

TInt CTZRulesCache::GetEncodedTimeZoneRulesSizeL(const TTime& aStartTime, const TTime& aEndTime, TTzTimeReference /*aTimeRef */)
	{
	// fetch a new set of rules from the data provider if the range of the rules in iEncodedRules does not
	// contain both aStartTime and aEndTime
	if (
		(iTimeZoneId->TimeZoneNumericID() != KUnknownTZId)
		&&
		( (iEncodedRules == NULL) || !(iEncodedRules->RulesApply(aStartTime)) || !(iEncodedRules->RulesApply(aEndTime) ) )
	    )
		{
        TInt startYear = aStartTime.DateTime().Year();
        TInt endYear = aEndTime.DateTime().Year();
		doGetEncodedTimeZoneRulesL(startYear, endYear);
		}

	if (iEncodedRules)
		{
		return iEncodedRules->Count() * sizeof (TTzRule) + sizeof (CTzRules);
		}
	else
		{
		return 0;
		}
	}

CVTzActualisedRules& CTZRulesCache::GetTimeZoneRulesL(const TTime& aTime,TTzTimeReference aTimerRef)
	{
    if ((iTimeZoneId->TimeZoneNumericID() != KUnknownTZId) 
		&& (!(RuleApplies(*iActualisedRules, aTime, aTimerRef))))
		{
		doGetActualisedTimeZoneRulesL(aTime);
		}
	return *iActualisedRules;
	}

/** 
Tests if the period covered by the cached rules applies to the supplied time.
*/
TBool CTZRulesCache::RuleApplies(CVTzActualisedRules& actRules, 
    const TTime& aTime, TTzTimeReference aTimerRef) const 
	{
	TInt startYear = actRules.StartYear();
	TInt endYear = actRules.EndYear();
	
	TDateTime dateTime(aTime.DateTime());
	
	__ASSERT_ALWAYS(aTimerRef!=ETzStdTimeReference, RTz::Panic(RTz::EPanicUnsupportedTimeReference));
	
	// aTime may be in UTC or wall time. If aTime is provided
	// in wall time the range which is checked will be reduced by one year
	// on either side, to avoid false inclusions. eg. If the time zone was
	// Japan and the date passed in here was Jan 1st at 6:00 local time,
	// that would be a different year in UTC time. This will result
	// in more fetches to the database in some cases, but the alternative
	// is to iterate through all the rules every time this method is called
	// to find the matching rule, which is not very efficent in itself.

	if (aTimerRef == ETzWallTimeReference)
		{
		// This reduces the range that is checked. For example, if the
		// actualised rules are from 2000 to 2006, this will
		// check Jan 2001 to Jan 2006
		startYear++;
		}
	else
		{
		// This checks the whole range. For example, if the
		// actualised rules are from 2000 to 2006, this will
		// check Jan 2000 to Jan 2007
		endYear++; 
		}
		
	const TTime KStart(TDateTime(startYear, EJanuary, 0, 0, 0, 0, 0));
	const TTime KEnd(TDateTime(endYear, EJanuary, 0, 0, 0, 0, 0));
	return ((aTime >= KStart) && (aTime < KEnd));
	}

//------------------------------------------------------------------------------------
//
CSystemTzRulesCache::~CSystemTzRulesCache()
	{
	// does nothing
	}


void CSystemTzRulesCache::SetDefaultZoneIdL()
	{
	// default to old TLocale DST Rule
	delete iTimeZoneId;
	iTimeZoneId = NULL;
	iTimeZoneId = CTzId::NewL(KUnknownTZName);
	iTimeZoneId->SetId(KUnknownTZId);

	// The UTC Offset combines both the standard and the dst offset
	TInt stdOffset = User::UTCOffset().Int() / KSecondsPerMinute;
		
	// create a default std only encoded rule
	delete iEncodedRules;
	iEncodedRules = NULL;
	iEncodedRules = CTzRules::NewL(KMinYear,KMaxYear);
	TTimeWithReference timeRef;
	TTimeWithReference timeRef2(TTimeWithReference::Max());
	TTzRule encodedRule(timeRef, timeRef2,stdOffset,stdOffset,EJanuary,ETzFixedDate,0,0,ETzWallTimeReference,0);
	iEncodedRules->AddRuleL(encodedRule);
	
	delete iActualisedRules;
	iActualisedRules = NULL;
	iActualisedRules = CVTzActualisedRules::NewL(KMinYear, KMaxYear);
    iEncodedRules->GetActualisedRulesL(*iActualisedRules);
	}


CSystemTzRulesCache* CSystemTzRulesCache::NewL(const CTzId& aTimeZoneId,
								CTzConfigAgent& aTimeZoneConfigAgent,
								const TTime& aTime)
	{
	CSystemTzRulesCache* self = new (ELeave) CSystemTzRulesCache(aTimeZoneConfigAgent);
	CleanupStack::PushL(self);

	self->ConstructL(aTimeZoneId, aTime);

	CleanupStack::Pop(self);
	return self;
	}



//------------------------------------------------------------------------------------
// CDstEventNotifier
//

/**
Creates a new instance of CDstEventNotifier on the heap.

@param aObserver the observer of the timer.
@return CDstEventNotifier* the instance created.
@internalComponent

*/
CDstEventNotifier* CDstEventNotifier::NewL(MDstEventObserver& aObserver)
    {
	CDstEventNotifier* self = new (ELeave) CDstEventNotifier(aObserver);
	CleanupStack::PushL(self);
	self->ConstructL();     // Construct CTimer.
	CActiveScheduler::Add(self);
	CleanupStack::Pop(self);
	return self;
    }
    
/**
Sets a new DST Change Timer at the given time.

@param aUtcTime the UTC time of the DST event.
@internalComponent

*/
void CDstEventNotifier::SetTimer(const TTime& aUtcTime)
    {
    // Cancel any existing timer
    Cancel();
	// Start the new timer.
	AtUTC(aUtcTime);
    }
    
/**
Constructor.

@param aObserver the observer of the timer.
@internalComponent

*/
CDstEventNotifier::CDstEventNotifier(MDstEventObserver& aObserver)
 :  CTimer(EPriorityStandard),
    iObserver(aObserver)
    {
    }

/**
Called when the timer completes. The observer is notified.

@internalComponent

*/
void CDstEventNotifier::RunL()
    {
    // Notify the observer.
    // Notifications based on the behaviour of RTimer

    switch (iStatus.Int())
        {
        case KErrNone:
            // The timer completed normally at the requested time.
            iObserver.HandleDstTimerCompleteL();
            break;
        case KErrAbort:
            // The timer was aborted because the system time changed.
			// This is taken care of by the environment change notifier
			// but is still handled to avoid falling into the default case 
			// and setting the alarm twice for each locale/time change.
            break;
        case KErrCancel:
            // The timer was cancelled.
            iObserver.HandleDstTimerCancellationL();
            break;
        default:
            // KErrUnderflow, KErrOverflow may be expected if an out-of-range timer is set.
            iObserver.DstTimerErrorOccurredL(iStatus.Int());
            break;
        }
    }
    
/**
Called if RunL leaves.

@internalComponent

*/
TInt CDstEventNotifier::RunError(TInt aError)
    {
    // Just return the error.
    return aError;
    }

/**
Cancels any existing timer.

@internalComponent

*/
void CDstEventNotifier::DoCancel()
    {
    // Cancel the timer.   
    CTimer::DoCancel();
    
    // Do not notify the observer here as it will create an infinite loop!
    // Cancellation of the RTimer is notified in RunL() with a status of KErrCancel.
    // That is what MDstEventObserver::HandleDstTimerCancellationL() is for.
    }
