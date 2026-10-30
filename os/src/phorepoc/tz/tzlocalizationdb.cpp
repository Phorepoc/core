// tzlocalizationdb.cpp -- the RCL_3 file + a cut-short localization database is rebuilt, not fatal to the server
// Copyright (c) 2008-2010 Nokia Corporation and/or its subsidiary(-ies).
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

#include "tzlocalizationdb.h"
#include "timezoneserver.h"
#include "mtimezonedataaccess.h"

#include <centralrepository.h>				// For CRepository.
#ifdef SYMBIAN_ENABLE_SPLIT_HEADERS
#include <tzusernames.h>//new file added for CTzUserNames which is publishedPartner
#endif
#include <tzlocalizedcityrecord.h>
#include <tzlocalizedtimezonerecord.h>

// Database location and access policy
_LIT(KTzLocalizationDbName,"c:TzLocalization.db");
_LIT(KTzLocalizationDbSecurityPolicy,"secure[10206A8B]");

//Table and Column names for Cached Time Zone Table
//These text strings are never visible and do not need localizing
_LIT(KCZTableName, 			"FrequentlyUsedZones");
_LIT(KCZTableTzIdCol, 		"TzId");
_LIT(KCZTableStdCol, 		"StandardName");
_LIT(KCZTableDSTCol, 		"DSTName");
_LIT(KCZTableShortStdCol,	"ShortStandardName");
_LIT(KCZTableShortDSTCol, 	"ShortDSTName");
_LIT(KCZTableCityCol, 		"City");
_LIT(KCZTableCityGroupCol, 	"GroupId");
_LIT(KCZTableResourceIdCol,	"ResourceId");
_LIT(KCZTableCityIndexCol,	"CityIndex");

//Table and Column names for User Added Cities Table
//These text strings are never visible and do not need localizing
_LIT(KUCTableName, 			"UserCities");
_LIT(KUCTableTzId, 			"TzId");
_LIT(KUCTableCity, 			"City");
_LIT(KUCTableGroup, 		"GroupId");
_LIT(KUCTableResourceId,	"ResourceId");

enum TTzZoneColumn
	{
	// Enum will used as a DB column index, so it must start at 1
	ETzZoneId = 1,
	ETzZoneStdName,
	ETzZoneDSTName,
	ETzZoneShortStdName,
	ETzZoneShortDSTName,
	ETzZoneCity,
	ETzZoneGroupId,
	ETzZoneResourceId,
	ETzZoneCityIndex
	};

_LIT(KTzMutexName, 	"TZ_GlobalMutex");

void CTzLocalizationDb::CleanupTimeZonePointerArray(TAny* aArray)
	{
	RPointerArray<CTzLocalizedTimeZoneRecord>* array = static_cast<RPointerArray<CTzLocalizedTimeZoneRecord>* >(aArray);
	if (array)
		{
		array->ResetAndDestroy();
		array->Close();
		}
	}

void CTzLocalizationDb::CleanupCityPointerArray(TAny* aArray)
	{
	RPointerArray<CTzLocalizedCityRecord>* array = static_cast<RPointerArray<CTzLocalizedCityRecord>* >(aArray);
	if (array)
		{
		array->ResetAndDestroy();
		array->Close();
		}
	}

CTzLocalizationDb* CTzLocalizationDb::NewL(CTzServer& aServer)
	{
	CTzLocalizationDb* self = new(ELeave) CTzLocalizationDb(aServer);
	CleanupStack::PushL(self);
	self->ConstructL();
	CleanupStack::Pop(self);
	return self;
	}

CTzLocalizationDb::~CTzLocalizationDb()
	{
	iServer.UserTimeZoneDb().RemoveObserver(this);
	CloseDb();
	iDbsSession.Close();
	}

CTzLocalizationDb::CTzLocalizationDb(CTzServer& aServer)
	: iServer(aServer)
	{
	}

void CTzLocalizationDb::ConstructL()
	{
	User::LeaveIfError(iDbsSession.Connect());
	OpenDbL();
	iServer.UserTimeZoneDb().AddObserverL(this);
	}

void CTzLocalizationDb::OpenDbL()
	{
	if (iZoneMutex.OpenGlobal(KTzMutexName) != KErrNone)
		{
		User::LeaveIfError(iZoneMutex.CreateGlobal(KTzMutexName)) ;
		}
	//Attempt to open the database
	TInt error = iLocalizedTimeZoneDb.Open(iDbsSession,KTzLocalizationDbName,KTzLocalizationDbSecurityPolicy);
	if (error == KErrNotFound)
		{
		// Database file doesn't exist.  Attempt to create a new one.
		CreateBlankDatabaseL();
		}
	else if ((error == KErrNone) && IsInvalidL())
		{
		RecreateBlankDatabaseL();
		}
	else if (error == KErrEof || error == KErrCorrupt)
		{
		// Phorepoc: a database file cut short (a reset while DBMS wrote it) failed every start of this boot-critical
		// server with KErrEof -- a restart loop on every boot. It is rebuilt like an invalid one.
		RecreateBlankDatabaseL();
		}
	else if (error == KErrNone && iServer.TimeZoneManager().TzDbHasChanged())
		{	
		// We have to refresh the data in the frequently used zones table
		// and the city table because it may contain the time zone id that
		// is not existing in the Tz database. Or the the details of a tz rule
		// has been changed.
		RestoreDbL();
		}
	else if (error != KErrNone)
		{
		User::Leave(error);
		}
	}

void CTzLocalizationDb::CloseDb()
    {
	iLocalizedTimeZoneDb.Close();
	if (iZoneMutex.Handle() && iZoneMutex.IsHeld())    // it is a leave, so release mutex
		{
		iZoneMutex.Signal();
		}       
	iZoneMutex.Close();
	}

void CTzLocalizationDb::CreateBlankDatabaseL()
	{
	User::LeaveIfError(iLocalizedTimeZoneDb.Create(iDbsSession,KTzLocalizationDbName,KTzLocalizationDbSecurityPolicy));
	CreateBlankFrequentlyUsedZoneTableL();
	CreateBlankUserCityTableL();
	}

/**
Creates the Cached Zone Table.
@return KErrNone if succesful, or one of the DBMS Leave codes
@internalTechnology
*/
void CTzLocalizationDb::CreateBlankFrequentlyUsedZoneTableL()
	{
	// Create the columns for the cached zones table
	RArray<TDbCol> cachedTableCols;
	CleanupClosePushL(cachedTableCols);
	cachedTableCols.AppendL(TDbCol(KCZTableTzIdCol,			EDbColUint16));
	cachedTableCols.AppendL(TDbCol(KCZTableStdCol, 			EDbColText));
	cachedTableCols.AppendL(TDbCol(KCZTableDSTCol, 			EDbColText));
	cachedTableCols.AppendL(TDbCol(KCZTableShortStdCol, 	EDbColText));
	cachedTableCols.AppendL(TDbCol(KCZTableShortDSTCol, 	EDbColText));
	cachedTableCols.AppendL(TDbCol(KCZTableCityCol, 		EDbColText));
	cachedTableCols.AppendL(TDbCol(KCZTableCityGroupCol, 	EDbColUint8));
	cachedTableCols.AppendL(TDbCol(KCZTableResourceIdCol, 	EDbColUint32));
    cachedTableCols.AppendL(TDbCol(KCZTableCityIndexCol,	EDbColInt32));

	// Create the columnset - add the columns
	// Columns MUST be added in the same order they appear in TTzZoneColumn
	CDbColSet* frequentlyUsedZoneColSet = CDbColSet::NewLC();
	TInt numCols = cachedTableCols.Count();
	for(TInt i = 0; i < numCols; ++i)
		{
		frequentlyUsedZoneColSet->AddL(cachedTableCols[i]);
		}

	// Create the Cached Time Zone table
	User::LeaveIfError(iLocalizedTimeZoneDb.CreateTable(KCZTableName,*frequentlyUsedZoneColSet));

	//Open the newly created table
	RDbView zoneView;
	CleanupClosePushL(zoneView);
	PrepareZoneViewL(zoneView);
	zoneView.Reset();
	//Populate with initial (blank) data.

	_LIT(KEmptyString," ");

	for (TInt x = 0; x < CTzLocalizedTimeZone::ECachedTimeZones; ++x)
		{
		iZoneMutex.Wait();
		CleanupStack::PushL(TCleanupItem(ReleaseMutex,&iZoneMutex));
		// Insert empty row
		zoneView.InsertL();
		// Fill the table with blank data
		zoneView.SetColL(ETzZoneId,			0);
		zoneView.SetColL(ETzZoneStdName,		KEmptyString);
		zoneView.SetColL(ETzZoneDSTName,		KEmptyString);
		zoneView.SetColL(ETzZoneShortStdName,	KEmptyString);
		zoneView.SetColL(ETzZoneShortDSTName,	KEmptyString);
		zoneView.SetColL(ETzZoneCity,			KEmptyString);
		zoneView.SetColL(ETzZoneGroupId,		0);
		zoneView.SetColL(ETzZoneResourceId,	0);
		zoneView.SetColL(ETzZoneCityIndex,		0);
		zoneView.PutL(); // Complete insertion
		CleanupStack::Pop() ;
		iZoneMutex.Signal() ;
		}
	
	CleanupStack::PopAndDestroy(&zoneView);
	CleanupStack::PopAndDestroy(frequentlyUsedZoneColSet);
	CleanupStack::PopAndDestroy(&cachedTableCols); //cachedTableCols
	}

/**
Creates the user added city database table.
@return KErrNone if succesful, or one of the DBMS Leave codes
@internalTechnology
*/
void CTzLocalizationDb::CreateBlankUserCityTableL()
	{
	//Create the columns for the user aded cities table
	RArray<TDbCol> cityTableCols;
	CleanupClosePushL(cityTableCols);
	cityTableCols.AppendL(TDbCol(KUCTableTzId,			EDbColUint16));
	cityTableCols.AppendL(TDbCol(KUCTableCity,			EDbColText));
	cityTableCols.AppendL(TDbCol(KUCTableGroup,			EDbColUint8));
	cityTableCols.AppendL(TDbCol(KUCTableResourceId,	EDbColUint32));

	// Create the columnset - add the columns
	// Columns MUST be added in the same order they appear in TTzCityColumn
	CDbColSet* userCityColSet = CDbColSet::NewLC();
	TInt numCols = cityTableCols.Count();
	for(TInt i = 0; i < numCols; ++i)
		{
		userCityColSet->AddL(cityTableCols[i]);
		}

	// Create the User City table
	User::LeaveIfError(iLocalizedTimeZoneDb.CreateTable(KUCTableName,*userCityColSet));
	
	CleanupStack::PopAndDestroy(userCityColSet);
	CleanupStack::PopAndDestroy(&cityTableCols);
	}

//Check if the database is corrupt or invalid.
TBool CTzLocalizationDb::IsInvalidL()
	{
	RDbView zoneView;
	CleanupClosePushL(zoneView);
	PrepareZoneViewL(zoneView);
	TBool isInvalid = EFalse;
	if (!zoneView.FirstL())
		{
		isInvalid = ETrue;
		}
	if (!isInvalid)
		{
		for (TInt i = 1; i < CTzLocalizedTimeZone::ECachedTimeZones; i++)
			{
		    if (!zoneView.NextL())
			    {
			    isInvalid = ETrue;
			    break;
		        }
		    }
		}
	CleanupStack::PopAndDestroy(&zoneView);
	return isInvalid;
	}

void CTzLocalizationDb::RecreateBlankDatabaseL()
	{
	iLocalizedTimeZoneDb.Close();
	const TUid KTzLocalizationDbSecurityPolicyUid = {0x10206A8B};
    User::LeaveIfError(iDbsSession.DeleteDatabase(KTzLocalizationDbName(),KTzLocalizationDbSecurityPolicyUid));
    CreateBlankDatabaseL();
	}

TBool CTzLocalizationDb::NeedsUpdatingL()
	{
	// Check the version of the database
    // There are 9 columns as of Oct 2006
	TBool result = ETrue;
	RDbView zoneView;
	CleanupClosePushL(zoneView);
	PrepareZoneViewL(zoneView);
    if (zoneView.ColCount() == ETzZoneCityIndex)
        {
        result = EFalse;
        }
    CleanupStack::PopAndDestroy(&zoneView);
    return result;
	}

void CTzLocalizationDb::NotifyUserTzRulesChange(TTzUserDataChange /*aChange*/)
	{
	// Do nothing as we don't use any rules information
	}

void CTzLocalizationDb::NotifyUserTzNamesChange(TTzUserDataChange aChange)
	{
	if (iLocked)
		{
		return;
		}
	
	if (aChange.iOperation == ETzUserDataCreated)
		{
		// Creation of new zones has no impact on existing zones
		return;
		}
	
	// Iterate through the tables and refresh contents if needed
	TRAPD(err, NotifyUserTzNamesChangeL(aChange));
	if (err != KErrNone)
		{
		// If there has been an error we can't guarantee the contents of the
		// database. Safest option is to lock it. The only way to unlock it
		// is to reboot the server.
		iLocked = ETrue;
		}
	}

void CTzLocalizationDb::NotifyTZDataStatusChangeL(RTz::TTzChanges aChange)
	{
	if (iLocked)
		{
		return;
		}
	
	if(aChange == RTz::ETZLocalizationDataChanged)
		{
		CTzSystemDataDb* systemDataDb = CTzSystemDataDb::NewLC();
		
		RPointerArray<CTzLocalizedTimeZoneRecord> frequentlyUsedZones;
		CleanupStack::PushL(TCleanupItem(CleanupTimeZonePointerArray,&frequentlyUsedZones));
		
		RPointerArray<CTzLocalizedCityRecord> cachedCities;
		CleanupStack::PushL(TCleanupItem(CleanupTimeZonePointerArray,&cachedCities));
			
		for (TInt i = CTzLocalizedTimeZone::ECurrentZone; i < CTzLocalizedTimeZone::ECachedTimeZones; ++i)
			{
			CTzLocalizedTimeZone::TTzFrequentlyUsedZone freqUsedZone = (CTzLocalizedTimeZone::TTzFrequentlyUsedZone)i;
			CTzLocalizedTimeZoneRecord* timeZoneRecord = ReadFrequentlyUsedZoneL(freqUsedZone);
			CleanupStack::PushL(timeZoneRecord);
			CTzLocalizedCityRecord* cityRecord = ReadCachedTimeZoneCityL(freqUsedZone);
			CleanupStack::PushL(cityRecord);
		
			// If a change occurred, check if the timezone exists in the resource file and refresh it
			// If not, delete it
			if(!ExistsInSystemDbL(timeZoneRecord->Id(), *systemDataDb) && !CTzId::IsUserTzId(timeZoneRecord->Id()))
				{	
				CleanupStack::PopAndDestroy(cityRecord);
				cityRecord = NULL;
				CleanupStack::PopAndDestroy(timeZoneRecord);
				timeZoneRecord = NULL;
				if(freqUsedZone != CTzLocalizedTimeZone::ECurrentZone)
					{
					// Put the default cached zone and its default city in the array instead
					timeZoneRecord = systemDataDb->ReadFrequentlyUsedZoneL(freqUsedZone);
					}
				else
					{
					// get the current zone from the time zone server
					TInt id = GetTimeZoneIdFromTzServerL(*systemDataDb);
					timeZoneRecord = systemDataDb->ReadTimeZoneL(id);
					}
				CleanupStack::PushL(timeZoneRecord);
				cityRecord = systemDataDb->ReadDefaultCityL(timeZoneRecord->Id());
				CleanupStack::PushL(cityRecord);
				
				cachedCities.AppendL(cityRecord);
				// ownership is transferred into the array
				CleanupStack::Pop(cityRecord);
				frequentlyUsedZones.AppendL(timeZoneRecord);
				// ownership is transferred into the array
				CleanupStack::Pop(timeZoneRecord);
				}	
			else
				{
				TUint tzid = timeZoneRecord->Id();
				CleanupStack::PopAndDestroy(cityRecord);
				cityRecord = NULL;
				CleanupStack::PopAndDestroy(timeZoneRecord);
				timeZoneRecord = NULL;
				UpdateTimeZoneAndCityRecordForSystemDataL(frequentlyUsedZones, cachedCities, tzid, *systemDataDb);	
				}
			}
		WriteAllFrequentlyUsedZonesL(frequentlyUsedZones, cachedCities);
	
		// Get the cities with the deleted time zone id
		RDbView cityView;
		CleanupClosePushL(cityView);
		_LIT(KSQLQueryLit,"SELECT * FROM UserCities");
		// Iterate through all the cities and delete them when
		// the corresponding tz id doesn't exist anymore
		PrepareCityViewL(cityView, KSQLQueryLit);
		// Loop through the rowset and delete the cities
		for (cityView.FirstL(); cityView.AtRow(); cityView.NextL())
			{
			cityView.GetL();
			if (!ExistsInSystemDbL(cityView.ColUint16(ETzCityId), *systemDataDb))
				{
				cityView.DeleteL();
				}
			}
		CleanupStack::PopAndDestroy(&cityView);
		CleanupStack::PopAndDestroy(&cachedCities); 
		CleanupStack::PopAndDestroy(&frequentlyUsedZones); 
		CleanupStack::PopAndDestroy(systemDataDb);
		}
	}

void CTzLocalizationDb::UpdateTimeZoneAndCityRecordForSystemDataL(RPointerArray<CTzLocalizedTimeZoneRecord>& aFrequentlyUsedZones, 
		RPointerArray<CTzLocalizedCityRecord>& aCachedCities, TInt aTzId, CTzSystemDataDb& aSystemDataDb)
	{
	CTzLocalizedTimeZoneRecord* timeZone = aSystemDataDb.ReadTimeZoneL(aTzId);
	CleanupStack::PushL(timeZone);

	CTzLocalizedCityRecord* city = aSystemDataDb.ReadDefaultCityL(aTzId);
	CleanupStack::PushL(city);
	
	aCachedCities.AppendL(city);
	// ownership is transferred into the array
	CleanupStack::Pop(city);
	aFrequentlyUsedZones.AppendL(timeZone);
	// ownership is transferred into the array
	CleanupStack::Pop(timeZone);
	}

TBool CTzLocalizationDb::ExistsInSystemDbL(TUint aTzId, CTzSystemDataDb& aSystemDataDb)
	{
	CTzLocalizedTimeZoneRecord* timeZoneRecord = NULL;
	TRAPD(err, timeZoneRecord = aSystemDataDb.ReadTimeZoneL(aTzId));
	delete timeZoneRecord;
	if(err == KErrNone)
		{
		return ETrue;
		}
	return EFalse;	
	}

/**
Reads all the user defined cities stored in the
database. If the database is empty the
returned array will contain no elements.
@param aCities Array of cities to add the user defined to.
@internalTechnology
*/
void CTzLocalizationDb::ReadCitiesL(RPointerArray<CTzLocalizedCityRecord>& aCities)
	{
	if (iLocked)
		{
		User::Leave(KErrLocked);
		}
	_LIT(KSQLQueryLit,"SELECT * FROM UserCities");
	DoReadCitiesL(aCities,KSQLQueryLit());
	}

void CTzLocalizationDb::ReadCitiesL(RPointerArray<CTzLocalizedCityRecord>& aCities, TInt aTimeZoneId)
	{
	if (iLocked)
		{
		User::Leave(KErrLocked);
		}
	// Assign basic SQL query literal
	_LIT(KSQLQueryLit,"SELECT * FROM UserCities WHERE TzId = ");
	// Assign a buffer with enough room to hold the query including the time zone id
	HBufC* sqlStr = HBufC::NewLC(KSQLQueryLit().Length() + EMaxTimeZoneIdStringLength);
	// Set the buffer to the start of the query
	*sqlStr = KSQLQueryLit;
	// Append the time zone ID
	sqlStr->Des().AppendNum(aTimeZoneId);

	DoReadCitiesL(aCities,*sqlStr);
	CleanupStack::PopAndDestroy(sqlStr);
	}

void CTzLocalizationDb::ReadCitiesInGroupL(RPointerArray<CTzLocalizedCityRecord>& aCities, TUint8 aGroupId)
	{
	if (iLocked)
		{
		User::Leave(KErrLocked);
		}
	// Assign basic SQL query literal
	_LIT(KSQLQueryLit,"SELECT * FROM UserCities WHERE GroupId = ");
	HBufC* sqlStr = HBufC::NewLC(KSQLQueryLit().Length() + EMaxGroupIdStringLength);
	// Set the buffer to the start of the query
	*sqlStr = KSQLQueryLit;
	// Append the time zone ID
	sqlStr->Des().AppendNum(aGroupId);

	DoReadCitiesL(aCities,*sqlStr);
	CleanupStack::PopAndDestroy(sqlStr);
	}

/**
Retrieves a cached zone from the database.
The calling function takes ownership of the returned zone
@param aFrequentlyUsedZone - The cached zone to return
@return a cached zone from the database
@internalTechnology
*/
CTzLocalizedTimeZoneRecord* CTzLocalizationDb::ReadFrequentlyUsedZoneL(TInt aFrequentlyUsedZone)
	{
	if (iLocked)
		{
		User::Leave(KErrLocked);
		}
	return DoReadFrequentlyUsedZoneL(aFrequentlyUsedZone);
	}

CTzLocalizedTimeZoneRecord* CTzLocalizationDb::DoReadFrequentlyUsedZoneL(TInt aFrequentlyUsedZone)
	{
	// You cannot pass ECachedTimeZones in as the argument, because it is only
	// used to keep count of how many cached zones there are.
	__ASSERT_ALWAYS(aFrequentlyUsedZone != CTzLocalizedTimeZone::ECachedTimeZones, User::Leave(KErrArgument));
	
	//Prepare a view of the zone table
	RDbView zoneView;
	CleanupClosePushL(zoneView);
	PrepareZoneViewL(zoneView);
	
	//Ensure the cursor is at the first row in the table.
	zoneView.FirstL();
	
	//Skip through the rows until we reach the row for the required zone
	for (TInt x = 0; x < aFrequentlyUsedZone; ++x)
		{
		zoneView.NextL();
		}
	
	//Get the row
	zoneView.GetL();
	
	//Create the new localized time zone from the row data
	CTzLocalizedTimeZoneRecord* localizedTimeZone = CTzLocalizedTimeZoneRecord::NewL(zoneView.ColUint16(ETzZoneId),
		zoneView.ColDes(ETzZoneStdName), zoneView.ColDes(ETzZoneDSTName), zoneView.ColDes(ETzZoneShortStdName),
		zoneView.ColDes(ETzZoneShortDSTName), zoneView.ColUint32(ETzZoneResourceId));
	
	CleanupStack::PopAndDestroy(&zoneView);
	return localizedTimeZone;
	}

/**
Retrieves the city used to select this time zone if set.  If the time zone was not originally
set using a city then the default city for the time zone will be returned instead.
@param aFrequentlyUsedZone - The cached zone to find the city for
@return	 selected city for the cached zone
@internalTechnology
*/
CTzLocalizedCityRecord* CTzLocalizationDb::ReadCachedTimeZoneCityL(TInt aFrequentlyUsedZone)
	{
	if (iLocked)
		{
		User::Leave(KErrLocked);
		}
	
	// You cannot pass ECachedTimeZones in as the argument, because it is only
	// used to keep count of how many cached zones there are.
	__ASSERT_ALWAYS(aFrequentlyUsedZone != CTzLocalizedTimeZone::ECachedTimeZones, User::Leave(KErrArgument));

	//Prepare a view of the zone table
	RDbView zoneView;
	CleanupClosePushL(zoneView);
	PrepareZoneViewL(zoneView);

	//Ensure the cursor is at the first row in the table.
	zoneView.FirstL();

	//Skip through the rows until we reach the row for the required zone
	for (TInt x = 0; x < aFrequentlyUsedZone; ++x)
		{
		zoneView.NextL();
		}

	//Get the row
	zoneView.GetL();

	//Create the new localized time zone from the row data
	TInt index = 0;
	// An extra column was added but we still have to cope with the old format as we
	// need to read the database contents when upgrading to the new format
	if (zoneView.ColCount() == ETzZoneCityIndex)
        {
        index = zoneView.ColInt32(ETzZoneCityIndex);
        }
	CTzLocalizedCityRecord* city = CTzLocalizedCityRecord::NewL(zoneView.ColDes(ETzZoneCity),
		zoneView.ColUint8(ETzZoneGroupId), index, zoneView.ColUint16(ETzZoneId),
		zoneView.ColUint32(ETzZoneResourceId));
    
	CleanupStack::PopAndDestroy(&zoneView);
	return city;
	}

void CTzLocalizationDb::WriteCityL(const TDesC& aCityName, TUint16 aCityTzId, TUint8 aCityGroupId, TUint aCityTzResourceId)
	{
	if (iLocked)
		{
		User::Leave(KErrLocked);
		}
	
	// Build a SQL query to check if the city name already exists in the database
	// with a matching timezoneID
	RDbView cityView;
	CleanupClosePushL(cityView);
	HBufC* sqlStr = GetCityQueryLC(aCityName, aCityTzId);
	PrepareCityViewL(cityView, *sqlStr);
	CleanupStack::PopAndDestroy(sqlStr);
	sqlStr = NULL;

	TInt cityCount = cityView.CountL();

	if (cityCount == 0)
		{
		//City was not found.  Add the new city to the database
		//Retrieve the CdbColSet from the view
		CDbColSet* cityColSet = cityView.ColSetL();
		CleanupStack::PushL(cityColSet);

		//Set the rowset cursor to the beginning of the rowset,
		//insert an empty row to write the city to
		cityView.Reset();
		cityView.InsertL();

		//Set the city information for the new row
		cityView.SetColL(ETzCityId,aCityTzId);
		cityView.SetColL(ETzCityCity,aCityName);
		cityView.SetColL(ETzCityGroup,aCityGroupId);
		cityView.SetColL(ETzCityResourceId,aCityTzResourceId);

		CleanupStack::PopAndDestroy(cityColSet);
		cityView.PutL(); // Complete insertion
		}
	else
		{
		//City already exists.
		User::Leave(KErrAlreadyExists);
		}
	CleanupStack::PopAndDestroy(&cityView);
	iZoneMutex.Wait();
	iLocalizedTimeZoneDb.Compact();
	iZoneMutex.Signal();
	}

void CTzLocalizationDb::DeleteCityL(const TDesC& aCityName, TUint16 aCityTzId)
	{
	if (iLocked)
		{
		User::Leave(KErrLocked);
		}
	
	//Build a SQL query to check if the city name and time zone ID exist in the database
	RDbView cityView;
	CleanupClosePushL(cityView);
	HBufC* sqlStr = GetCityQueryLC(aCityName, aCityTzId);
	PrepareCityViewL(cityView, *sqlStr);
	CleanupStack::PopAndDestroy(sqlStr);
	sqlStr = NULL;
	
	TInt cityCount = cityView.CountL();

	if (cityCount > 0)
		{
		//A city with this name exists in the database.  Delete it.
		if (cityView.FirstL())
			{
			cityView.DeleteL();
			//Reset the cursor, as it now points to an invalid row
			cityView.Reset();
			}
		}
	else
		{
		//City was not found
		User::Leave(KErrNotFound);
		}
	CleanupStack::PopAndDestroy(&cityView);
	iZoneMutex.Wait();
	iLocalizedTimeZoneDb.Compact();
	iZoneMutex.Signal();
	}

/**
Stores the localized time zone in the database for easy retrieval.
The database keeps track of the two most recently used zones, these are updated every time
a new zone is set.  The zone to be replaced becomes recently used zone 1, and recently used zone 1 becomes
recently used zone 2.  The old recently used zone 2 is discarded.
@param Any city in the time zone to store
@param aFrequentlyUsedZone - specifies which time zone to overwrite in the database
@internalTechnology
*/
void CTzLocalizationDb::WriteFrequentlyUsedZoneL(const CTzLocalizedTimeZoneRecord& aTimeZone, 
		const CTzLocalizedCityRecord& aCity, TInt aFrequentlyUsedZone)
	{
	if (iLocked)
		{
		User::Leave(KErrLocked);
		}
	
	// Only the Current, Home and Zone of Interest can be changed explicitly
	// using this function.  Recent Zones are automatically updated.
	__ASSERT_ALWAYS(aFrequentlyUsedZone < CTzLocalizedTimeZone::ERecentZone1, User::Leave(KErrArgument));
	
	iZoneMutex.Wait();
	
	RDbView zoneView;
	CleanupClosePushL(zoneView);
	PrepareZoneViewL(zoneView);
	//Ensure the cursor is at the first row in the table.
	zoneView.FirstL();
	TInt x;
	//Skip through the rows until we reach the row for the zone to update
	for (x = 0; x < aFrequentlyUsedZone; ++x)
		{
		zoneView.NextL();
		}

	//Get the row
	zoneView.GetL();
	
	//Bookmark the row
	TDbBookmark rowToUpdate(zoneView.Bookmark());

	//Insert a copy of the row - this is now the last row in the table, and will become
	//recently used zone 1 after the original recently used zones are deleted

	CleanupStack::PushL(TCleanupItem(ReleaseMutex,&iZoneMutex));
	zoneView.InsertCopyL();
	zoneView.PutL();
	CleanupStack::Pop();
	
	//Position the cursor back at the row to update
	zoneView.GotoL(rowToUpdate);

	//Update the row with the new data
	DoWriteFrequentlyUsedZoneL(zoneView, aTimeZone, aCity);

	//Reset the cursor back to the first row in the table.
	zoneView.FirstL();

	//Skip through the rows until we reach the row for recent zone 1
	for (x = 0; x < CTzLocalizedTimeZone::ERecentZone1; ++x)
		{
		zoneView.NextL();
		}

	//Get the row
	zoneView.GetL();

	//Bookmark the row
	TDbBookmark recentZone1(zoneView.Bookmark());

	//Insert a copy of the row - this is now the last row in the table, and will become
	//recently used zone 2 after the original recently used zones are deleted
	CleanupStack::PushL(TCleanupItem(ReleaseMutex,&iZoneMutex));
	zoneView.InsertCopyL();
	zoneView.PutL();
	CleanupStack::Pop();

	//Reset the cursor to the orignal recent zone 1 position
	zoneView.GotoL(recentZone1);

	//Delete the original recent zone 1 and recent zone 2.
	//The new recent zone 1 and recent zone 2 are moved back into the correct
	//position by the removal of the preceding rows
	CleanupStack::PushL(TCleanupItem(ReleaseMutex,&iZoneMutex));
	zoneView.DeleteL();
	CleanupStack::Pop();

	zoneView.NextL();

	CleanupStack::PushL(TCleanupItem(ReleaseMutex,&iZoneMutex));
	zoneView.DeleteL();
	CleanupStack::Pop();
	
	//Cursor is now invalid (pointing to an empty row)
	//and shuold be reset to a known state before reusing
	//Cleanup
	CleanupStack::PopAndDestroy(&zoneView);
	iLocalizedTimeZoneDb.Compact();
	iZoneMutex.Signal();
	}

/**
Sets all the cached zones and their associated cities to be those that are
supplied.
@param aTimeZones Array of time zones to be written.
@param aCities Array of cities to be written.
@internalTechnology
*/
void CTzLocalizationDb::WriteAllFrequentlyUsedZonesL(const RPointerArray<CTzLocalizedTimeZoneRecord>& aTimeZones, 
		const RPointerArray<CTzLocalizedCityRecord>& aCities)
	{
	if (iLocked)
		{
		User::Leave(KErrLocked);
		}
	DoWriteAllFrequentlyUsedZonesL(aTimeZones, aCities);
	}

void CTzLocalizationDb::DoWriteAllFrequentlyUsedZonesL(const RPointerArray<CTzLocalizedTimeZoneRecord>& aTimeZones, 
		const RPointerArray<CTzLocalizedCityRecord>& aCities)
	{
	// Ensure that the numbers of items in the arrays are the expected amount
	__ASSERT_ALWAYS(aTimeZones.Count() == CTzLocalizedTimeZone::ECachedTimeZones &&
					aCities.Count() == CTzLocalizedTimeZone::ECachedTimeZones, User::Leave(KErrArgument));
	
	iZoneMutex.Wait();	
		
	RDbView zoneView;
	CleanupClosePushL(zoneView);
	PrepareZoneViewL(zoneView);
	//Ensure the cursor is at the first row in the table.
	zoneView.FirstL();

	//Skip through the rows until we reach the row for the zone to update
	for (TInt i = 0; i < CTzLocalizedTimeZone::ECachedTimeZones; ++i)
		{
		DoWriteFrequentlyUsedZoneL(zoneView, *aTimeZones[i], *aCities[i]);
		zoneView.NextL();
		}

	CleanupStack::PopAndDestroy(&zoneView);
	
	iLocalizedTimeZoneDb.Compact();
	iZoneMutex.Signal();
	}

/**
Opens the a view on the city table and returns a reference to it.
The view should be released after use with a call to CloseCityView()
@param aSql A sql string to prepare the view with
@return reference to opened iCityTable
@internalTechnology
*/
void CTzLocalizationDb::PrepareCityViewL(RDbView& aCityView, const TDesC& aSqlQuery)
	{
	User::LeaveIfError(aCityView.Prepare(iLocalizedTimeZoneDb, TDbQuery(aSqlQuery),	aCityView.EUpdatable));
	User::LeaveIfError(aCityView.EvaluateAll());
	}

/**
Opens a view to the zone table
@internalTechnology
*/
void CTzLocalizationDb::PrepareZoneViewL(RDbView& aZoneView)
	{
	_LIT(KReadZoneView,"SELECT * FROM FrequentlyUsedZones");
	User::LeaveIfError(aZoneView.Prepare(iLocalizedTimeZoneDb, TDbQuery(KReadZoneView),	aZoneView.EUpdatable));
	User::LeaveIfError(aZoneView.EvaluateAll());
	}

/**
Returns the SQL string that will pick the specified city out of the user-added
cities database. Leaves the returned HBufC pointer on the Cleanup Stack.
The query is of the form:
	"SELECT * FROM UserCities WHERE City = 'London' AND TZID = 2592"
@param aCity The city to make the query for.
@return The SQL Query that will select the aCity form the DB
@internalTechnology
*/
HBufC* CTzLocalizationDb::GetCityQueryLC(const TDesC& aCityName, TInt aCityTimeZoneId)
	{
	// Assign the start and middle of the basic SQL query literal 
	_LIT(KSQLQueryLitStart,"SELECT * FROM UserCities WHERE City = '");
	_LIT(KSQLQueryLitMiddle,"' AND TZID = ");
	
	/* Assign a buffer with enough room to hold the query including the city 
	   name and the time zone id */
	TInt maxBufferLength = KSQLQueryLitStart().Length() + KSQLQueryLitMiddle().Length() 
							+ aCityName.Length() + EMaxTimeZoneIdStringLength;
	HBufC* sqlStr = HBufC::NewLC(maxBufferLength);
	
	// Append the beginning of the query
	sqlStr->Des().Append(KSQLQueryLitStart);
	// Append the city name into the query
	sqlStr->Des().Append(aCityName);
	// Append the middle of the query
	sqlStr->Des().Append(KSQLQueryLitMiddle);
	// Append the time zone ID into the query
	sqlStr->Des().AppendNum(aCityTimeZoneId);
	
	return sqlStr;
	}

HBufC* CTzLocalizationDb::GetCityQueryLC(TInt aCityTimeZoneId)
	{
	// Assign the start and middle of the basic SQL query literal 
	_LIT(KSQLQueryLitStart,"SELECT * FROM UserCities WHERE TZID = ");
	
	/* Assign a buffer with enough room to hold the query including the city 
	   name and the time zone id */
	TInt maxBufferLength = KSQLQueryLitStart().Length() + EMaxTimeZoneIdStringLength;
	HBufC* sqlStr = HBufC::NewLC(maxBufferLength);
	
	// Append the beginning of the query
	sqlStr->Des().Append(KSQLQueryLitStart);
	// Append the time zone ID into the query
	sqlStr->Des().AppendNum(aCityTimeZoneId);
	
	return sqlStr;
	}

/**
Reads the user defined cities in the database
matched by the SQL query into the supplied array. If the
database is empty the returned array will contain no elements.
@param aCities array of cities to add the user defined to.
@param aSqlString - SQL search string determining the cities to retrieve
@internalTechnology
*/
void CTzLocalizationDb::DoReadCitiesL(RPointerArray<CTzLocalizedCityRecord>& aCities, const TDesC& aSqlString)
	{
	RDbView cityView;
	CleanupClosePushL(cityView);
	PrepareCityViewL(cityView, aSqlString);
	
	//Loop through the rowset
	for (cityView.FirstL(); cityView.AtRow(); cityView.NextL())
		{
		// Fetch a cached copy of current row
		cityView.GetL();

		//Create the new city from the row data
		CTzLocalizedCityRecord* newCity = CTzLocalizedCityRecord::NewLC(cityView.ColDes(ETzCityCity), 
			cityView.ColUint8(ETzCityGroup), aCities.Count(), cityView.ColUint16(ETzCityId),
			cityView.ColUint32(ETzCityResourceId));
     
		//Append the new city to the city array - aCities takes ownership of newCity
		aCities.AppendL(newCity);
		CleanupStack::Pop(newCity);
		}
	CleanupStack::PopAndDestroy(&cityView);
	}

/**
Does the actual writing of a zone and city into the database. iZoneView must
have already been prepared and moved to the correct offset into the row set
before this function is called. e.g. at least the following should have been
done:
	PrepareZoneViewL();
	iZoneView.FirstL();
Otherwise the cursor position may be invalid and a DBMS panic will occur.
@param aTimeZone The time zone to write.
@param aCity The city to write.
@param aFrequentlyUsedZone The enum of the cached zone to write to.
@internalTechnology
*/
void CTzLocalizationDb::DoWriteFrequentlyUsedZoneL(RDbView& aZoneView, const CTzLocalizedTimeZoneRecord& aTimeZone,
		const CTzLocalizedCityRecord& aCity)
	{
	CleanupStack::PushL(TCleanupItem(ReleaseMutex,&iZoneMutex));
	aZoneView.UpdateL();  
	aZoneView.SetColL(ETzZoneId, (TInt)aTimeZone.Id());
	aZoneView.SetColL(ETzZoneStdName, aTimeZone.StandardName());
	aZoneView.SetColL(ETzZoneDSTName, aTimeZone.DaylightName());
	aZoneView.SetColL(ETzZoneShortStdName, aTimeZone.ShortStandardName());
	aZoneView.SetColL(ETzZoneShortDSTName, aTimeZone.ShortDaylightName());
	aZoneView.SetColL(ETzZoneCity, aCity.Name());
	aZoneView.SetColL(ETzZoneGroupId, aCity.GroupId());
	aZoneView.SetColL(ETzZoneResourceId, aTimeZone.ResourceId());
    if (aZoneView.ColCount() == ETzZoneCityIndex)
        {
        aZoneView.SetColL(ETzZoneCityIndex, aCity.Index());
        }
    aZoneView.PutL();
	CleanupStack::Pop();
	}

TInt CTzLocalizationDb::GetTimeZoneIdFromTzServerL(CTzSystemDataDb& aSystemDataDb)
	{
	// Get current time zone using the current CTzId from the time zone server
	TUint timeZoneIdInt = iServer.TimeZoneManager().GetTimeZoneIdL().TimeZoneNumericID();
	
	// Is the time zone ID the unknown/invalid ID?
	// This is temporary measure that is required until PREQ 234' TzServer
	// changes are introduced
	const TUint32 KUnknownTZId = 0x0ffffff0;
	if((TUint32)timeZoneIdInt == KUnknownTZId)
		{
		// Return the ID of the DEFAULT home zone instead
		CTzLocalizedTimeZoneRecord* homeZone = aSystemDataDb.ReadFrequentlyUsedZoneL(CTzLocalizedTimeZone::EHomeZone);
		CleanupStack::PushL(homeZone);
		timeZoneIdInt = homeZone->Id();
		CleanupStack::PopAndDestroy(homeZone);
		}
		
	return timeZoneIdInt;
	}

void CTzLocalizationDb::NotifyUserTzNamesChangeL(TTzUserDataChange aChange)
	{
	// If an existing user-defined time zone is updated it means we may have
	// to refresh the data in the frequently used zones table but the city table
	// only contains the time zone id so there is nothing to update in there.
	
	// If an existing user-defined time zone is deleted it means we may have
	// to refresh the data in the frequently used zones table and the city table
	// because it may contain the time zone id that has been deleted.
	CTzSystemDataDb* systemDataDb = CTzSystemDataDb::NewLC();
	TBool frequentlyUsedZonesNeedUpdating = EFalse;
	RPointerArray<CTzLocalizedTimeZoneRecord> frequentlyUsedZones;
	CleanupStack::PushL(TCleanupItem(CleanupTimeZonePointerArray,&frequentlyUsedZones));
	RPointerArray<CTzLocalizedCityRecord> cachedCities;
	CleanupStack::PushL(TCleanupItem(CleanupCityPointerArray,&cachedCities));
	for (TInt i = CTzLocalizedTimeZone::ECurrentZone; i < CTzLocalizedTimeZone::ECachedTimeZones; ++i)
		{
		CTzLocalizedTimeZone::TTzFrequentlyUsedZone freqUsedZone = (CTzLocalizedTimeZone::TTzFrequentlyUsedZone)i;
		CTzLocalizedTimeZoneRecord* timeZoneRecord = ReadFrequentlyUsedZoneL(freqUsedZone);
		CleanupStack::PushL(timeZoneRecord);
		CTzLocalizedCityRecord* cityRecord = ReadCachedTimeZoneCityL(freqUsedZone);
		CleanupStack::PushL(cityRecord);
		
	 if (timeZoneRecord->Id() == aChange.iTzId || cityRecord->TzId() == aChange.iTzId)
		{
		if (aChange.iOperation == ETzUserDataUpdated)
			{
			CleanupStack::PopAndDestroy(cityRecord);
			cityRecord = NULL;
			CleanupStack::PopAndDestroy(timeZoneRecord);
			timeZoneRecord = NULL;
			UpdateTimeZoneAndCityRecordL(frequentlyUsedZones, cachedCities, aChange.iTzId);
			}
		else if (aChange.iOperation == ETzUserDataDeleted)
			{
			CleanupStack::PopAndDestroy(cityRecord);
			cityRecord = NULL;
			CleanupStack::PopAndDestroy(timeZoneRecord);
			timeZoneRecord = NULL;
			
			if(freqUsedZone != CTzLocalizedTimeZone::ECurrentZone)
				{
				// Put the default cached zone and its default city in the array instead
				timeZoneRecord = systemDataDb->ReadFrequentlyUsedZoneL(freqUsedZone);
				}
			else
				{
				// get the current zone from the time zone server
				TInt id = GetTimeZoneIdFromTzServerL(*systemDataDb);
				timeZoneRecord = systemDataDb->ReadTimeZoneL(id);
				}
			CleanupStack::PushL(timeZoneRecord);
			cityRecord = systemDataDb->ReadDefaultCityL(timeZoneRecord->Id());
			CleanupStack::PushL(cityRecord);
			
			cachedCities.AppendL(cityRecord);
			// ownership is transferred into the array
			CleanupStack::Pop(cityRecord);
			frequentlyUsedZones.AppendL(timeZoneRecord);
			// ownership is transferred into the array
			CleanupStack::Pop(timeZoneRecord);
			}
		frequentlyUsedZonesNeedUpdating = ETrue;
		}
	else
		{
		cachedCities.AppendL(cityRecord);
		// ownership is transferred into the array
		CleanupStack::Pop(cityRecord);
		frequentlyUsedZones.AppendL(timeZoneRecord);
		// ownership is transferred into the array
		CleanupStack::Pop(timeZoneRecord);
		}
	}
			
	if (frequentlyUsedZonesNeedUpdating)
		{
		WriteAllFrequentlyUsedZonesL(frequentlyUsedZones, cachedCities);
		}
		
	if (aChange.iOperation == ETzUserDataDeleted)
		{
		DeleteCitiesL(aChange);
		iZoneMutex.Wait();
		iLocalizedTimeZoneDb.Compact();
		iZoneMutex.Signal();
		}
	CleanupStack::PopAndDestroy(&cachedCities); 
	CleanupStack::PopAndDestroy(&frequentlyUsedZones);
	CleanupStack::PopAndDestroy(systemDataDb);
	}

void CTzLocalizationDb::UpdateTimeZoneAndCityRecordL(RPointerArray<CTzLocalizedTimeZoneRecord>& aFrequentlyUsedZones, RPointerArray<CTzLocalizedCityRecord>& aCachedCities, TInt aTzId)
	{
	CTzUserNames* names = iServer.UserTimeZoneDb().ReadTzNamesL(aTzId);
	CleanupStack::PushL(names);

	CTzLocalizedTimeZoneRecord* timeZoneRecord = CTzLocalizedTimeZoneRecord::NewL(aTzId, names->StandardName(),
		names->DaylightSaveName(), names->ShortStandardName(), names->ShortDaylightSaveName(), 0);
	CleanupStack::PushL(timeZoneRecord);
	
	CTzLocalizedCityRecord* cityRecord = CTzLocalizedCityRecord::NewL(names->CityName(), 0, 0, aTzId, 0);
	CleanupStack::PushL(cityRecord);

	aCachedCities.AppendL(cityRecord);
	// ownership is transferred into the array
	CleanupStack::Pop(cityRecord);
	aFrequentlyUsedZones.AppendL(timeZoneRecord);
	// ownership is transferred into the array
	CleanupStack::Pop(timeZoneRecord);
	CleanupStack::PopAndDestroy(names);
	}

void CTzLocalizationDb::DeleteCitiesL(TTzUserDataChange aChange)
	{
	// Get the cities with the deleted time zone id
	RDbView cityView;
	CleanupClosePushL(cityView);
	if(aChange.iOperation == ETzUserDataDeleted)
		{
		HBufC* sqlStr = GetCityQueryLC(aChange.iTzId);
		PrepareCityViewL(cityView, *sqlStr);
		CleanupStack::PopAndDestroy(sqlStr);
		sqlStr = NULL;
		}
	else if(aChange.iOperation == ETzUserDataRestored)
		{
		_LIT(KSQLQueryLit,"SELECT * FROM UserCities");
		// Iterate through all the cities and delete them when
		// the corresponding tz id doesn't exist anymore
		PrepareCityViewL(cityView, KSQLQueryLit);
		}
	// Loop through the rowset and delete the cities
	for (cityView.FirstL(); cityView.AtRow(); cityView.NextL())
		{
		if(aChange.iOperation == ETzUserDataDeleted)
			{
			cityView.DeleteL();		
			}
		else if(aChange.iOperation == ETzUserDataRestored)
			{
			cityView.GetL();
			if (!iServer.TimeZoneManager().TzDataProvider().IsIdInDbL(cityView.ColUint16(ETzCityId)))
				{
				cityView.DeleteL();
				}
			}	
		}
	CleanupStack::PopAndDestroy(&cityView);
	}

/**
Releases the mutex if leave occurs between wait and signal.
@internalTechnology
*/
void CTzLocalizationDb::ReleaseMutex(TAny* iTarget)
	{
	RMutex* mutex = static_cast<RMutex*>(iTarget);	
	mutex->Signal();	
	}

// Backup and restore operations
void CTzLocalizationDb::BackupBeginningL()
	{
	CloseDb();
	iLocked = ETrue;
	}

void CTzLocalizationDb::BackupCompletedL()
	{
	OpenDbL();
	iLocked = EFalse;
	}

void CTzLocalizationDb::RestoreBeginningL()
	{
	CloseDb();
	iLocked = ETrue;
	}

void CTzLocalizationDb::RestoreDbL()
	{
	// When the Tz server is connected or a restore completes, we have to
	// refresh the data in the frequently used zones table and the city table
	// because it may contain the time zone id that has been deleted.  Or if the
	// time zone id is still there but the details of the rule has been changed.

	CTzSystemDataDb* systemDataDb = CTzSystemDataDb::NewLC();

	TBool frequentlyUsedZonesNeedUpdating = EFalse;

	RPointerArray<CTzLocalizedTimeZoneRecord> frequentlyUsedZones;
	CleanupStack::PushL(TCleanupItem(CleanupTimeZonePointerArray,&frequentlyUsedZones));

	RPointerArray<CTzLocalizedCityRecord> cachedCities;
	CleanupStack::PushL(TCleanupItem(CleanupCityPointerArray,&cachedCities));

	for (TInt i = CTzLocalizedTimeZone::ECurrentZone; i < CTzLocalizedTimeZone::ECachedTimeZones; ++i)
		{//Loop through each record in frequently used table
		CTzLocalizedTimeZone::TTzFrequentlyUsedZone freqUsedZone = (CTzLocalizedTimeZone::TTzFrequentlyUsedZone)i;
		CTzLocalizedTimeZoneRecord* timeZoneRecord = DoReadFrequentlyUsedZoneL(freqUsedZone);
		TUint tzid = timeZoneRecord->Id();
		delete timeZoneRecord;
		TBool tzIdExistInDb = iServer.TimeZoneManager().TzDataProvider().IsIdInDbL(tzid);
		
		if(!tzIdExistInDb)
			{
			if(freqUsedZone == CTzLocalizedTimeZone::ECurrentZone)
				{
				tzid = GetTimeZoneIdFromTzServerL(*systemDataDb);
				}
			else
				{
				tzid = GetFrequentlyUsedDefaultZoneIdL(freqUsedZone, *systemDataDb);
				}
			}
		
		if (CTzId::IsUserTzId(tzid))
			{
			UpdateTimeZoneAndCityRecordL(frequentlyUsedZones, cachedCities, tzid);	
			}
		else
			{
			UpdateTimeZoneAndCityRecordForSystemDataL(frequentlyUsedZones, cachedCities, tzid, *systemDataDb);
			}

		frequentlyUsedZonesNeedUpdating = ETrue;
		}
			
	if (frequentlyUsedZonesNeedUpdating)
		{
		//Update the frequently used zone table
		DoWriteAllFrequentlyUsedZonesL(frequentlyUsedZones, cachedCities);
		}
	
	TTzUserDataChange change;
	change.iOperation = ETzUserDataRestored;
	change.iTzId = KInvalidTzId;
	//Delete the city infomation if the associated time zone no longer exist.
	DeleteCitiesL(change);
	
	CleanupStack::PopAndDestroy(&cachedCities); 
	CleanupStack::PopAndDestroy(&frequentlyUsedZones);
	CleanupStack::PopAndDestroy(systemDataDb);
	}

/**
Get the localized default time zone Id for the given frequently used time zone.
The default time zone for each of the possible frequently used time zones may be
specified in the TZ Server repository or in the TZ Localization resource file.
Check the possible sources in this order.

@param aFreqUsedZone The frequently used time zone for which the localized
default time zone is required.
@param aSystemDataDb For TZ Localization database access.

@return The localized default time zone Id for the given frequently used time zone.
*/
TInt CTzLocalizationDb::GetFrequentlyUsedDefaultZoneIdL(
	CTzLocalizedTimeZone::TTzFrequentlyUsedZone aFreqUsedZone,
	CTzSystemDataDb& aSystemDataDb)
	{
	// Assume that we will not find the key in the repository or that we do find
	// the key but it has no value.  If either of these scenarios is true then
	// we will use the time zone identifier recorded in the resource file for
	// the default time zone.
	
	TUint32 defaultTimeZoneKey = GetFrequentlyUsedDefaultZoneCenRepKeyL(aFreqUsedZone);
	CRepository* tzRepository = CRepository::NewL(NTzUpdate::KPropertyCategory);
	CleanupStack::PushL(tzRepository);
	TInt defaultTimeZoneId = 0;
	TInt err = tzRepository->Get(defaultTimeZoneKey, defaultTimeZoneId);
	CleanupStack::PopAndDestroy(tzRepository);
	
	if(err != KErrNotFound)
		{
		User::LeaveIfError(err);
		}
	
	if(defaultTimeZoneId == 0)
		{
		defaultTimeZoneId = aSystemDataDb.ReadFrequentlyUsedZoneIdL(aFreqUsedZone);
		}
	
	return defaultTimeZoneId;
	}

/**
Get the TZ Server's CenRep repository key for the given frequently used zone.

@param aFreqUsedZone The frequently used time zone for which the TZ Server's
CenRep repository key is required.

@return The TZ Server's CenRep repository key for the given frequently used
zone.
*/
TUint32 CTzLocalizationDb::GetFrequentlyUsedDefaultZoneCenRepKeyL(CTzLocalizedTimeZone::TTzFrequentlyUsedZone aFreqUsedZone)
	{
	// These key values are copied from those defined in tzconfigagent.h.  We
	// want to keep the key values private - this is one way to do it.
	const TUint32 KDefaultHomeTimeZoneKey = 0x3UL;
	const TUint32 KDefaultInterestTimeZoneKey = 0x4UL;
	const TUint32 KDefaultRecent1TimeZoneKey = 0x5UL;
	const TUint32 KDefaultRecent2TimeZoneKey = 0x6UL;

	TUint32 key = 0;

	switch(aFreqUsedZone)
	    {
	    case CTzLocalizedTimeZone::EHomeZone:
	    	{
	    	key = KDefaultHomeTimeZoneKey;
	        }
	    break;

	    case CTzLocalizedTimeZone::EInterestZone:
	    	{
	    	key = KDefaultInterestTimeZoneKey;
	        }
	    break;

	    case CTzLocalizedTimeZone::ERecentZone1:
	    	{
	    	key = KDefaultRecent1TimeZoneKey;
	        }
	    break;

	    case CTzLocalizedTimeZone::ERecentZone2:
	    	{
	    	key = KDefaultRecent2TimeZoneKey;
	        }
	    break;
	    
	    default:
	    	{
	    	User::Leave(KErrArgument);
	    	}
	    }

	return key;
    }

void CTzLocalizationDb::RestoreCompletedL()
	{
	OpenDbL();
	iLocked = EFalse;
	RestoreDbL();
	}

