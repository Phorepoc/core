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
//
// Description:
// apacmdln.cpp
//

#ifdef SYMBIAN_ENABLE_SPLIT_HEADERS
#if !defined(__APA_INTERNAL_H__)
#include "apainternal.h"
#endif
#endif //SYMBIAN_ENABLE_SPLIT_HEADERS

#include <apacmdln.h>
#include "APASTD.H" // Panics etc.
#include <s32mem.h>

#include <e32svr.h>

// comand line tokens
const TUint KApaCommandLetterOpen='O';
const TUint KApaCommandLetterCreate='C';
const TUint KApaCommandLetterRun='R';
const TUint KApaCommandLetterBackground='B';
const TUint KApaCommandLetterViewActivate='V';
const TUint KApaCommandLetterRunWithoutViews='W';
const TUint KApaCommandLetterBackgroundAndWithoutViews='A';

_LIT(KLitTokenServerDifferentiator, "-srvDfr=");
_LIT(KLitTokenDefaultScreenNumber, "-dsc=");
_LIT(KLitTokenParentWindowGroupID, "-prntwgid=");
_LIT(KLitTokenDebugMemFail, "-debugMemFail:");
_LIT(KLitTokenAppStartupInstrumentationEventIdBase, "-appStartupInstrEvIdBase=");
_LIT(KLitTokenOpaqueData, "-opaque=");

enum TProcessEnvironmentSots 
	{
	EEnvironmentSlotUnused = 0,
	EEnvironmentSlotMain = 1,
	EEnvironmentSlotFsSession = 2,
	EEnvironmentSlotFile = 3,
	EFirstEnvironmentSlotForPublicUse = 8,
	ENumberOfEnvironmentSlotsForPublicUse = 4
	};

enum TIpcMessageSlots
	{
	EIpcSlotMain = 0,
	EIpcSlotFsSession = 1,
	EIpcSlotFile = 2
	};


// CApaCommandLine

/** Private c'tor - initialize using NewLC()
@internalTechnology */
CApaCommandLine::CApaCommandLine() : 
	iCommand(EApaCommandRun), iServerDifferentiator(0), iDefaultScreenNumber(KErrNotFound),
	iParentWindowGroupID(0), iDebugMemFail(0), iAppStartupInstrumentationEventIdBase(0),
	iFile(), iParentProcessId(KNullProcessId)
	{
	}

/** Destructor.
Frees resources owned by the object prior to deletion. */
EXPORT_C CApaCommandLine::~CApaCommandLine()
	{
	iDocumentName.Close();
	iExecutableName.Close();
	iOpaqueData.Close();
	iTailEnd.Close();
	iFile.Close();
	}

/** Creates an empty command line object.
@return A pointer to the new command line object. */
EXPORT_C CApaCommandLine* CApaCommandLine::NewL()
	{
	CApaCommandLine* self = CApaCommandLine::NewLC();
	CleanupStack::Pop();
	return self;
	}

/** Creates an empty command line object, and puts a pointer to it onto the cleanup stack.
@return A pointer to the new command line object. */
EXPORT_C CApaCommandLine* CApaCommandLine::NewLC()
	{
	CApaCommandLine* self = new(ELeave) CApaCommandLine;
	CleanupStack::PushL(self);
	return self;
	}

EXPORT_C void CApaCommandLine::SetDocumentNameL(const TDesC& aDocName)
/** Sets the document name from the specified descriptor.

If the document name has embedded spaces, then it must be enclosed within 
quotation marks.

@param aDocName A descriptor containing the document name. */
	{
	RBuf documentName;
	documentName.CreateL(aDocName);
	
	iDocumentName.Close();	// free any existing memory
	iDocumentName.Assign(documentName);	
	}

/** Gets the document name from the launch information.
@return A pointer descriptor representing the document name. The document 
name is returned without any enclosing quotation marks. If the launch information 
contains no document name, then the pointer descriptor has zero length. */
EXPORT_C TPtrC CApaCommandLine::DocumentName() const
	{
	return iDocumentName;
	}

/** Sets the executable name from the specified descriptor.
@param aExecutableName A descriptor containing the executable name. */
EXPORT_C void CApaCommandLine::SetExecutableNameL(const TDesC& aExecutableName)
	{
	RBuf executableName;
	executableName.CreateL(aExecutableName);
	
	iExecutableName.Close();	// free any existing memory
	iExecutableName.Assign(executableName);
	}

/** Gets the executable name from the launch information.
@return A pointer descriptor representing the executable name. */
EXPORT_C TPtrC CApaCommandLine::ExecutableName() const
	{
	return iExecutableName;
	}

/** Sets the opaque-data from the specified 8-bit descriptor.

This is called internally and populated from the data in the application's registration resource file, 
i.e. from the resource indicated by the opaque_data field of the APP_REGISTRATION_INFO resource (if the 
opaque_data field was non-zero).

@param aOpaqueData An 8-bit descriptor containing the opaque-data. 
*/
EXPORT_C void CApaCommandLine::SetOpaqueDataL(const TDesC8& aOpaqueData)
	{
	RBuf8 opaqueData;
	opaqueData.CreateL(aOpaqueData);
	
	iOpaqueData.Close();	// free any existing memory
	iOpaqueData.Assign(opaqueData);
	}

/** Gets the opaque-data from the launch information.

See the description of SetOpaqueDataL. By default, this attribute is an empty descriptor.

@see SetOpaqueDataL
@return An 8-bit pointer descriptor representing the opaque-data. 
*/
EXPORT_C TPtrC8 CApaCommandLine::OpaqueData() const
	{
	return iOpaqueData;
	}

/** Sets the command code.

The command code is used to indicate how an application is to be launched.

@see TApaCommand
@param aCommand The command code. */
EXPORT_C void CApaCommandLine::SetCommandL(TApaCommand aCommand)
	{
	iCommand = aCommand;
	}

/** Gets the command code from the launch information.

See the description of SetCommandL.

@see SetCommandL
@see TApaCommand
@return The command code. */
EXPORT_C TApaCommand CApaCommandLine::Command() const
	{
	return iCommand;
	}

/** Sets the trailing data.

The UI Framework provides a specific set of pre-defined command line 
options. Additional user defined or pre-defined command line options, 
may be passed to an application by setting the TailEnd.

@param aTailEnd An 8 bit descriptor containing the trailing data. 
@publishedAll */
EXPORT_C void CApaCommandLine::SetTailEndL(const TDesC8& aTailEnd)
	{
	RBuf8 tailEnd;
	tailEnd.CreateL(aTailEnd);
	
	iTailEnd.Close();	// free any existing memory
	iTailEnd.Assign(tailEnd);
	}

/** Gets the trailing data from the launch information.

See the description of SetTailEndL.

@see SetTailEndL
@return A pointer descriptor representing the trailing data. If the launch 
information contains no trailing data, then the pointer descriptor has zero 
length. 
@publishedAll */
EXPORT_C TPtrC8 CApaCommandLine::TailEnd() const
	{
	return iTailEnd;
	}

/** Sets the file to be passed into the application (by handle). 
This will then be retrieved by that application-process calling GetFileByHandleL().

@publishedPartner
@released
@param aFile The file object to be passed into the application. No transfer of this 
object's ownership takes place in this function. */
EXPORT_C void CApaCommandLine::SetFileByHandleL(const RFile& aFile)
	{
	__ASSERT_ALWAYS(aFile.SubSessionHandle() != KNullHandle, Panic(EPanicInvalidHandle));
	__ASSERT_ALWAYS(iFile.SubSessionHandle() == KNullHandle, Panic(EPanicHandleAlreadySet));
	
	User::LeaveIfError(iFile.Duplicate(aFile));
	}

/** Opens (by handle) the file that was passed into SetFileByHandleL by the process launching the local application.

On entering this function, aFile must be non-open. It is recommended that aFile is 
pushed onto the cleanup-stack (via CleanupClosePushL()) before this function is called.

@publishedPartner
@released
@param aFile The file object to be set up from the handle passed into the application. The 
caller has the responsibility to Close() this object, even if this function leaves. */
EXPORT_C void CApaCommandLine::GetFileByHandleL(RFile& aFile) const
	{
	__ASSERT_ALWAYS(aFile.SubSessionHandle()==KNullHandle, Panic(EPanicHandleAlreadySet));

	if (iFile.SubSessionHandle() != KNullHandle)
		User::LeaveIfError(aFile.Duplicate(iFile));
	}

/** Assigns a command line to a process (EKA2 only). 

This replaces the EKA1 method which involved retrieving the full command line (using 
CApaCommandLine::FullCommandLine()) and passing it to the process (or thread on the 
emulator).

This function is used as follows (the order of the first 2 steps is irrelevant):-
- create the process and load the executable (RProcess::Create()),
- create the command line object (CApaCommandLine::NewLC()), and set it up using 
the various setter functions, for instance SetDocumentNameL(),
- call SetProcessEnvironmentL() to assign the command line to the process,
- call Resume() on the process.

Note that this sequence of steps bypasses the application architecture, and is 
not recommended. RApaLsSession::StartApp() is the recommended way to 
launch an application with a command line.

@param aProcess The process to which the command line is assigned.
@leave KErrNotSupported This indicates that the function was called on EKA1.
@see RApaLsSession::StartApp()
*/
EXPORT_C void CApaCommandLine::SetProcessEnvironmentL(RProcess& aProcess) const
	{
	HBufC8* const streamableAttributes = StreamableAttributesLC();
	User::LeaveIfError(aProcess.SetParameter(EEnvironmentSlotMain, *streamableAttributes));
	CleanupStack::PopAndDestroy(streamableAttributes);

	if (iFile.SubSessionHandle() != KNullHandle)
		User::LeaveIfError(iFile.TransferToProcess(aProcess, EEnvironmentSlotFsSession, EEnvironmentSlotFile));
	}

/**
@internalTechnology
*/
EXPORT_C void CApaCommandLine::GetIpcArgsLC(TIpcArgs& aIpcArgs) const
	{
	aIpcArgs.Set(EIpcSlotMain, StreamableAttributesLC());
	if (iFile.SubSessionHandle() != KNullHandle)
		User::LeaveIfError(iFile.TransferToServer(aIpcArgs, EIpcSlotFsSession, EIpcSlotFile));
	}

/**
@internalTechnology
*/
HBufC8* CApaCommandLine::StreamableAttributesLC() const
	{
	CBufFlat* const buffer = CBufFlat::NewL(128);
	CleanupStack::PushL(buffer);
	
	RBufWriteStream writeStream;
	writeStream.Truncate(*buffer);
	ExternalizeL(writeStream);
	writeStream.CommitL();
	HBufC8* const bufferAsDescriptor = buffer->Ptr(0).AllocL();
	
	CleanupStack::PopAndDestroy(buffer);
	
	CleanupStack::PushL(bufferAsDescriptor);
	return bufferAsDescriptor;
	}

/** Acts as a second constructor and completes a commandline object from 
the aMessage object.
@internalTechnology */
EXPORT_C void CApaCommandLine::ConstructCmdLineFromMessageL(const RMessage2& aMessage)
	{
	// Create a buffer of the right size and get the data from the RMessage2
	RBuf8 buffer;
	buffer.CleanupClosePushL();
	buffer.CreateL(aMessage.GetDesLengthL(EIpcSlotMain));
	aMessage.ReadL(EIpcSlotMain, buffer);
	
	// Create a stream and use it to read the data from the buffer
	RDesReadStream stream;
	CleanupClosePushL(stream);
	stream.Open(buffer);
	InternalizeL(stream);
	CleanupStack::PopAndDestroy();	// stream
	
	CleanupStack::PopAndDestroy();	// buffer
	iFile.AdoptFromClient(aMessage, EIpcSlotFsSession, EIpcSlotFile); // ignore the returned error - assume it means that no file has been passed across
	}

/** Constructs a command line object.

If command line information is provided in the environment-slots it creates command line object from
process environment-slots, else creates it from the information returned by User::CommandLine().

It can be called from a context where there is no CTrapCleanup.

Calling this function more than once in a process is not supported and will result in an empty command
line being returned. If an application wants to inspect any part of its command line, it 
should override CEikAppUi::ProcessCommandParametersL(CApaCommandLine& aCommandLine) and call the base
class implementation if required.

@see CEikAppUi::ProcessCommandParametersL(CApaCommandLine& aCommandLine).
@param aCommandLine On return, a pointer to a newly constructed command line object.
@return KErrNone, if successful; otherwise one of the other system-wide error codes.
@internalTechnology */
EXPORT_C TInt CApaCommandLine::GetCommandLineFromProcessEnvironment(CApaCommandLine*& aCommandLine)
	{ // static
	aCommandLine = NULL;
	CApaCommandLine* const commandLine = new CApaCommandLine;
	if(!commandLine)
		return KErrNoMemory;
	
	CTrapCleanup* trapCleanup = NULL;
	if (!User::TrapHandler())
		{
		trapCleanup = CTrapCleanup::New(); // we're being called from an environment without a cleanup-stack, so temporarily create one here
		if(!trapCleanup)
			{
			delete commandLine;
			return KErrNoMemory;
			}
		}
	
	TRAPD(error, commandLine->DoGetCommandLineFromProcessEnvironmentL());
	aCommandLine = commandLine;
	delete trapCleanup;
	return error;
	}

void CApaCommandLine::DoGetCommandLineFromProcessEnvironmentL()
	{
	const TInt bufLen = User::ParameterLength(EEnvironmentSlotMain);
	if (bufLen == KErrNotFound)
		{
		RBuf commandLineString;
		commandLineString.CleanupClosePushL();
		commandLineString.CreateL(User::CommandLineLength());
		User::CommandLine(commandLineString);
		User::LeaveIfError(DoGetParametersFromCommandLineString(commandLineString));
		CleanupStack::PopAndDestroy();	// commandLineString
		}
	else
		{
		User::LeaveIfError(bufLen); // in case bufLen is some error other than KErrNotFound
		RBuf8 buffer;
		buffer.CleanupClosePushL();
		buffer.CreateL(bufLen);
		
		User::LeaveIfError(User::GetDesParameter(EEnvironmentSlotMain, buffer));
		RDesReadStream stream;
		CleanupClosePushL(stream);
		stream.Open(buffer);
		InternalizeL(stream);
		CleanupStack::PopAndDestroy();	// stream
		
		CleanupStack::PopAndDestroy();	// buffer
		}

	iFile.AdoptFromCreator(EEnvironmentSlotFsSession, EEnvironmentSlotFile); // ignore the returned error - assume it means that no file has been passed across
	}

/** Sets the Parent Process ID for the Child Process launched with this command line.

This establishes a Parent-Child relationship which ensures that the child process is
terminated when the parent terminates.

@param aProcessId The Process ID. */
EXPORT_C void CApaCommandLine::SetParentProcessId(TProcessId aProcessId)
	{
	iParentProcessId = aProcessId;
	}

/** Gets the Parent Process ID of the Child Process launched with this command line.

See the description of SetParentProcessId.

@see SetParentProcessId
@return The Parent Process ID. */
EXPORT_C TProcessId CApaCommandLine::ParentProcessId() const
	{
	return iParentProcessId;
	}

void CApaCommandLine::ExternalizeL(RWriteStream& aStream) const
	{
	// iFile is not supported via RReadStream/RWriteStream
	aStream << DocumentName();
	aStream << ExecutableName();
	aStream << OpaqueData();
	aStream << TailEnd();
	aStream.WriteInt32L(iCommand);
	aStream.WriteInt32L(iServerDifferentiator);
	aStream.WriteInt32L(iDefaultScreenNumber);
	aStream.WriteInt32L(iParentWindowGroupID);
	aStream.WriteInt32L(iDebugMemFail);
	aStream.WriteInt32L(iAppStartupInstrumentationEventIdBase);
	aStream.WriteInt32L(iParentProcessId);
	}

void CApaCommandLine::InternalizeL(RReadStream& aStream)
	{
	// iFile is not supported via RReadStream/RWriteStream
	const TInt KMaxBufLength = 4000;
	iDocumentName.Close();	// free any existing memory
	iDocumentName.CreateL(aStream, KMaxBufLength);
	iExecutableName.Close();	// free any existing memory
	iExecutableName.CreateL(aStream, KMaxBufLength);
	iOpaqueData.Close();	// free any existing memory
	iOpaqueData.CreateL(aStream, KMaxBufLength);
	iTailEnd.Close();	// free any existing memory
	iTailEnd.CreateL(aStream, KMaxBufLength);
	iCommand = static_cast<TApaCommand>(aStream.ReadInt32L());
	iServerDifferentiator = aStream.ReadInt32L();
	iDefaultScreenNumber = aStream.ReadInt32L();
	iParentWindowGroupID = aStream.ReadInt32L();
	iDebugMemFail = aStream.ReadInt32L();
	iAppStartupInstrumentationEventIdBase = aStream.ReadInt32L();
	iParentProcessId = aStream.ReadInt32L();
	}

/** Sets that no server is required.

The value of server differentiator is set to zero, to indicate that no server
is required.

See the description of SetServerRequiredL.
@see SetServerRequiredL
*/
EXPORT_C void CApaCommandLine::SetServerNotRequiredL()
	{
	SetServerDifferentiatorL(0);
	}

/** Sets the required server.

The server differentiator is a number generated by the client that helps to uniquely 
identify the server. It is used by an application to indicate whether a server should
be created and how it should be named.

@param aServerDifferentiator A differentiator for the required server.
@see REikAppServiceBase::LaunchAppL() 
*/
EXPORT_C void CApaCommandLine::SetServerRequiredL(TUint aServerDifferentiator)
	{
	SetServerDifferentiatorL(aServerDifferentiator);
	}

/**
@see REikAppServiceBase::LaunchAppL()
@internalTechnology
*/
void CApaCommandLine::SetServerDifferentiatorL(TUint aServerDifferentiator)
	{
	iServerDifferentiator = aServerDifferentiator;
	}
	
/** Gets the server differentiator.

See the description of SetServerRequiredL.

@see SetServerRequiredL
@return The non-zero differentiator for the server, else zero indicating a server 
is not required.
@see REikAppServiceBase::LaunchAppL() */
EXPORT_C TUint CApaCommandLine::ServerRequired() const
	{
	return iServerDifferentiator;
	}

/** Provides support for devices with more than one screen.  A number representing the default
or startup screen may be passed to an application.
Screen numbers and characteristics are defined in the window server initialisation 
file (wsini.ini).

@param aDefaultScreenNumber The number of the default (startup) screen.
@publishedAll */
EXPORT_C void CApaCommandLine::SetDefaultScreenL(TInt aDefaultScreenNumber)
	{
	__ASSERT_ALWAYS(aDefaultScreenNumber>=0, Panic(EPanicInvalidScreenNumber));
	iDefaultScreenNumber = aDefaultScreenNumber;
	}

/** Extracts and returns the default (startup) screen that was specified in the command line.

@return	A number representing the default (startup) screen.  0 (Zero) if nothing present.
@publishedAll */
EXPORT_C TInt CApaCommandLine::DefaultScreen() const
	{
	return Max(0, iDefaultScreenNumber);
	}
	
/**
@publishedAll
*/
EXPORT_C TBool CApaCommandLine::IsDefaultScreenSet() const
	{
	return (iDefaultScreenNumber != KErrNotFound);
 	}

/** Sets the ID of the parent window-group - the application should create its own 
window-group as a child off this parent.

@param aParentWindowGroupID The ID of the parent window-group - the application 
should create its window-group as a child off this parent. */
EXPORT_C void CApaCommandLine::SetParentWindowGroupID(TInt aParentWindowGroupID)
	{
	iParentWindowGroupID = aParentWindowGroupID;
	}

/** Returns the ID of the parent window-group - the application should create its own 
window-group as a child of this parent.

@return The ID of the parent window-group - the application should create its 
window-group as a child off this . */
EXPORT_C TInt CApaCommandLine::ParentWindowGroupID() const
	{
	return iParentWindowGroupID;
	}

/** @internalAll */
EXPORT_C void CApaCommandLine::SetDebugMemFailL(TInt aDebugMemFail)
	{
	iDebugMemFail = aDebugMemFail;
	}

/** @internalAll */
EXPORT_C TInt CApaCommandLine::DebugMemFail() const
	{
	return iDebugMemFail;
	}

/** @internalAll */
EXPORT_C void CApaCommandLine::SetAppStartupInstrumentationEventIdBaseL(TInt aAppStartupInstrumentationEventIdBase)
	{
	iAppStartupInstrumentationEventIdBase = aAppStartupInstrumentationEventIdBase;
	}

/** @internalAll */
EXPORT_C TInt CApaCommandLine::AppStartupInstrumentationEventIdBase() const
	{
	return iAppStartupInstrumentationEventIdBase;
	}

/** Returns the index of a process environment-slot for public use (in other words, 
one that is not used internally by CApaCommandLine). The number of slots available 
for public use is returned from NumberOfEnvironmentSlotsForPublicUse(), (this value 
may be increased over time). The returned value can then be passed into any of the 
Open(TInt,...) functions on RSessionBase, RMutex, RChunk, RCondVar, etc, or into 
User::GetTIntParameter(), User::GetDesParameter(), etc, depending on the type 
of the object in that environment slot.

@param aIndex The logical index of the public environment-slot. This must be greater 
than or equal to zero, and less than the value returned from NumberOfEnvironmentSlotsForPublicUse().
@return The physical index of an environment-slot in the local process. 
@publishedAll
*/
EXPORT_C TInt CApaCommandLine::EnvironmentSlotForPublicUse(TInt aIndex)
	{ // static
	__ASSERT_ALWAYS((aIndex>=0) && (aIndex < ENumberOfEnvironmentSlotsForPublicUse), Panic(EPanicEnvironmentSlotNotForPublicUse));
	return EFirstEnvironmentSlotForPublicUse + aIndex;
	}

/**
The number of process environment-slot available for public use.
@publishedAll
*/
EXPORT_C TInt CApaCommandLine::NumberOfEnvironmentSlotsForPublicUse()
	{
	return ENumberOfEnvironmentSlotsForPublicUse;
	}

// For use in CApaCommandLine::DoGetParametersFromCommandLineString() only.
struct SOption
	{
	const TDesC* iToken;
	TInt* iResult;
	TRadix iRadix;
	HBufC8* iHBufC8Result;
	};

TInt CApaCommandLine::DoGetParametersFromCommandLineString(const TDesC& aCmdLine)
// does the opposite of SetCmdLineL, i.e. sets iDocumentName, iExecutableName, iTailEnd, iCommand, iServerDifferentiator, iDefaultScreenNumber, iParentWindowGroupID , iDebugMemFail & iAppStartupInstrumentationEventIdBase from aCmdLine
// also sets iOpaqueData
	{
	const TInt cmdLength = aCmdLine.Length();
	TInt endLibNameOffset = cmdLength-1;
	TInt endDocNameOffset = cmdLength-1;

	// these variables are all "shadows" of member variables - we'll set the member variables corresponding to these at the end of this function, once all memory-allocation has succeeded, to make this function atomic	
	HBufC* documentName = NULL;
	HBufC* executableName = NULL;
	HBufC8* tailEnd = NULL;
	HBufC8* opaqueData = NULL;
	
	TApaCommand command = EApaCommandRun;
	TInt serverDifferentiator = 0;
	TInt defaultScreenNumber = KErrNotFound;
	TInt parentWindowGroupID = 0;
	TInt debugMemFail = 0;
	TInt appStartupInstrumentationEventIdBase = 0;
	TInt notUsed = 0;

	// Look for the name of the executable
	executableName = NameOfExecutable(aCmdLine, endDocNameOffset);
	if (!executableName)
	{
	delete executableName;
	return KErrNoMemory;	
	}
	
	// Work out the type of command		
	const TInt offset = endDocNameOffset-endLibNameOffset;
	if (offset > 1)
		{
		const TChar commandLetter = aCmdLine[endLibNameOffset+2];
		switch (commandLetter)
			{
		case KApaCommandLetterOpen:
			command = EApaCommandOpen;
			break;
		case KApaCommandLetterCreate:
			command = EApaCommandCreate;
			break;
		case KApaCommandLetterViewActivate:
			command = EApaCommandViewActivate;
			break;
		case KApaCommandLetterRunWithoutViews:
			command = EApaCommandRunWithoutViews;
			break;
		case KApaCommandLetterBackgroundAndWithoutViews:
			command = EApaCommandBackgroundAndWithoutViews;
			break;
		case KApaCommandLetterRun:
		default:
			break;
		case KApaCommandLetterBackground:
			command = EApaCommandBackground;
			break;
			}

	// Get the name of the document file, if any.
	if (offset > 2)
		{
		const TInt documentNameStartPosition = endLibNameOffset+3;
		documentName = StripQuotes(aCmdLine.Mid(documentNameStartPosition, (endDocNameOffset+1)-documentNameStartPosition)).Alloc();
		if (!documentName)
			{
			delete executableName;
			delete documentName;
			return KErrNoMemory;			
			}
		}
	}
	
	// Translate the command line tokens into their corresponing options
	const TInt KNumberOfSupportedOptions = 6;
	TFixedArray<SOption, KNumberOfSupportedOptions> optionArray;
	optionArray[0].iToken = &KLitTokenServerDifferentiator;
	optionArray[0].iResult = &serverDifferentiator;
	optionArray[0].iRadix = EDecimal;
	optionArray[1].iToken = &KLitTokenDefaultScreenNumber;
	optionArray[1].iResult = &defaultScreenNumber;
	optionArray[1].iRadix = EDecimal;
	optionArray[2].iToken = &KLitTokenParentWindowGroupID;
	optionArray[2].iResult = &parentWindowGroupID;
	optionArray[2].iRadix = EDecimal;
	optionArray[3].iToken = &KLitTokenDebugMemFail;
	optionArray[3].iResult = &debugMemFail;
	optionArray[3].iRadix = EHex;
	optionArray[4].iToken = &KLitTokenAppStartupInstrumentationEventIdBase;
	optionArray[4].iResult = &appStartupInstrumentationEventIdBase;
	optionArray[4].iRadix = EDecimal;
	optionArray[5].iToken = &KLitTokenOpaqueData;
	optionArray[5].iResult = &notUsed;
	optionArray[5].iRadix = EDecimal; // should not used if the command-line is well-formed
	
	TLex lex(aCmdLine.Mid(endDocNameOffset+1));
	lex.Mark();
	for (TInt optionIndex = 0; optionIndex < KNumberOfSupportedOptions; ++optionIndex)
		{
		lex.SkipSpace();
		const SOption& option = optionArray[optionIndex];
		const TPtrC remainder(lex.Remainder());
		__ASSERT_DEBUG(option.iToken, Panic(EDPanicInvalidToken));
		const TInt tokenLength = option.iToken->Length();
		if ((remainder.Length() >= tokenLength) && (remainder.Left(tokenLength).CompareF(*option.iToken) == 0))
			{
			if (option.iToken == &KLitTokenOpaqueData)
				{
				TInt endOfOpaqueDataIndex = 0;			
				for (TInt i = tokenLength; i < remainder.Length(); ++i)
					{
					const TChar current = remainder[i];
					if (current == ' ')
						{
						endOfOpaqueDataIndex = i;
						break; // parse no further
						}
					}
					
				if(endOfOpaqueDataIndex > tokenLength)
					{
					const TInt opaqueDataLength = endOfOpaqueDataIndex - tokenLength;
					delete opaqueData;
					opaqueData = TPtrC8(reinterpret_cast<const TUint8*>(remainder.Mid(tokenLength, opaqueDataLength).Ptr()),opaqueDataLength*sizeof(TText)).Alloc();
					if (!opaqueData)
						{
						delete executableName;
						delete documentName;
						delete opaqueData;
						return KErrNoMemory;						
						}
					
					lex.Inc(tokenLength + opaqueDataLength);
					lex.Mark();
					}
				else
					{
					delete opaqueData;
					delete documentName;
					delete executableName;
					delete tailEnd;
					// invalid command line. copy TLex.Val behavior
					return KErrGeneral;
					}
				}
			else
				{
				ASSERT(option.iResult);
				const TInt originalValue = *option.iResult;
				lex.Inc(tokenLength);
				TUint16 val = static_cast<TUint16> (*option.iResult);
				if (lex.Val(val, option.iRadix) == KErrNone)
					lex.Mark();
				else
					*option.iResult = originalValue;
				}
			}
		}
		
	lex.UnGetToMark();
	lex.SkipSpace();
	
	// Get the tail end
	const TPtrC remainder(lex.Remainder());
	const TInt lengthOfRemainder = remainder.Length();
	if (lengthOfRemainder > 0)
		{
		tailEnd = TPtrC8(reinterpret_cast<const TUint8*>(remainder.Ptr()),lengthOfRemainder*sizeof(TText)).Alloc();
		if (!tailEnd)
			{
			delete executableName;
			delete documentName;
			delete opaqueData;
			delete tailEnd;
			return KErrNoMemory;		
			}
		}

	// Free any existing memory
	iDocumentName.Close();
	iExecutableName.Close();
	iTailEnd.Close();
	iOpaqueData.Close();
	
	// Set the member variables, as all memory-allocations have succeeded.
	iDocumentName.Assign(documentName);
	iExecutableName.Assign(executableName);
	iTailEnd.Assign(tailEnd);
	iOpaqueData.Assign(opaqueData);
	iCommand = command;
	iServerDifferentiator = serverDifferentiator;
	iDefaultScreenNumber = defaultScreenNumber;
	iParentWindowGroupID = parentWindowGroupID;
	iDebugMemFail = debugMemFail;
	iAppStartupInstrumentationEventIdBase = appStartupInstrumentationEventIdBase;
	
	return KErrNone;
	}

/**
Get the name of the executable from the command line string.
@internalTechnology
*/
HBufC* CApaCommandLine::NameOfExecutable(const TDesC& aCmdLine, TInt& aEndDocNameOffset)
	{
	const TInt cmdLength = aCmdLine.Length();

	TBool openQuote = EFalse;
	TBool foundEndLibName = EFalse;
	for (TInt i = 0; i < cmdLength; ++i)
		{
		const TChar current = aCmdLine[i];
		if (current=='"')
			{
			openQuote = !openQuote;
			continue;
			};
			
		if ((current==' ') && !openQuote)
			{
			// space found outside of quotes
			if (foundEndLibName)
				{
				aEndDocNameOffset = i-1;
				break; // parse no further
				}
				
			aEndDocNameOffset = i-1;
			foundEndLibName = ETrue;
			}
		}
		
	// PHOREPOC board delta (S4.7): `cond ? <TPtrC prvalue> : <const TDesC& lvalue>` -- modern GCC types the
	// conditional as a TDesC16 prvalue, i.e. a SLICED copy holding only the type/length word; Alloc() then read the
	// missing iPtr from the stack (logs.exe "-logsbootup": fault address 0x1000000b = EPtrC | length 11). RVCT bound
	// it as intended. The same expression with the pointer descriptor kept whole:
	const TPtrC name(aEndDocNameOffset > -1 ? StripQuotes(aCmdLine.Left(aEndDocNameOffset+1)) : TPtrC(KNullDesC));
	return name.Alloc();
	}

/**
Strip all quates from the string.
*/
TPtrC CApaCommandLine::StripQuotes(const TDesC& aDes)
	//
	// return aDes stripped of any enclosing quotes 
	//
	{
	TInt start = 0;
	TInt end = aDes.Length()-1;
	TPtrC ret;
	if (end >= 0)
		{
		if (aDes[0] == '"')
			start++;

		if (aDes[end] == '"')
			end--;

		const TInt length = end-start+1;
		if (length > 0)
			ret.Set(aDes.Mid(start, length));
		}
	
	return ret;
	}

