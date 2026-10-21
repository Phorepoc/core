// toolchain/overlay/MsgMediaInfo.inl -- OURS: the release .inl (angle-included by MsgMediaInfo.h, so this copy wins)
// with one fix: FullFilePath() returned "iFullFilePath ? iFullFilePath->Des() : KNullDesC()", a TDesC VALUE (C++
// [expr.cond]) -- GCC sliced the TPtr and the path lost its pointer (MMS/uni editor attachments). Same bug as
// toolchain/shims/aifw_board, 2026-09-30.
/*
* Copyright (c) 2005 Nokia Corporation and/or its subsidiary(-ies).
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
* Description: MsgMediaInfo.inl*
*/



// -----------------------------------------------------------------------------
// CMsgMediaInfo::FullFilePath
// ACCESSOR
// -----------------------------------------------------------------------------
//
inline TPtrC CMsgMediaInfo::FullFilePath() const
    {
    return iFullFilePath
        ? TPtrC( *iFullFilePath )
        : TPtrC( KNullDesC );
    }

// -----------------------------------------------------------------------------
// CMsgMediaInfo::FileSize
// ACCESSOR
// -----------------------------------------------------------------------------
//
inline TInt CMsgMediaInfo::FileSize() const
    {
    return iFileSize;
    }

// -----------------------------------------------------------------------------
// CMsgMediaInfo::FileTime
// ACCESSOR
// -----------------------------------------------------------------------------
//
inline void CMsgMediaInfo::ModificationTime( TTime& aTime ) const
    {
    aTime = iModificationTime;
    }

// -----------------------------------------------------------------------------
// CMsgMediaInfo::MimeType
// ACCESSOR
// -----------------------------------------------------------------------------
//
inline TPtrC8 CMsgMediaInfo::MimeType() const
    {
    return iMimeType.Des8();
    }

// -----------------------------------------------------------------------------
// CMsgMediaInfo::MediaType
// ACCESSOR
// -----------------------------------------------------------------------------
//
inline TMsgMediaType CMsgMediaInfo::MediaType() const
    {
    return iMediaType;
    }

// -----------------------------------------------------------------------------
// CMsgMediaInfo::ProtectionStatus
// ACCESSOR
// -----------------------------------------------------------------------------
//
inline TUint32 CMsgMediaInfo::Protection() const
    {
    return iProtection;
    }

// -----------------------------------------------------------------------------
// CMsgMediaInfo::SetProtectionStatus
// MUTATOR
// -----------------------------------------------------------------------------
//
inline void CMsgMediaInfo::SetProtection( TUint32 aProtection )
    {
    iProtection = aProtection;
    }

// -----------------------------------------------------------------------------
// CMsgMediaInfo::DRMContentURI
// ACCESSOR
// -----------------------------------------------------------------------------
//
inline HBufC8* CMsgMediaInfo::DRMContentURI() const
    {
    return iContentURI;
    }

// -----------------------------------------------------------------------------
// CMsgMediaInfo::SetDRMContentURI
// MUTATOR
// -----------------------------------------------------------------------------
//
inline void CMsgMediaInfo::SetDRMContentURI( HBufC8* aContentURI )
    {
    iContentURI = aContentURI;
    }

// -----------------------------------------------------------------------------
// CMsgMediaInfo::Duration
// ACCESSOR
// -----------------------------------------------------------------------------
TInt CMsgMediaInfo::Duration() const
    {
    return iDuration;
    }

// -----------------------------------------------------------------------------
// CMsgMediaInfo::Parsed
// ACCESSOR
// -----------------------------------------------------------------------------
TBool CMsgMediaInfo::Parsed() const
    {
    return iParsed;
    }

// -----------------------------------------------------------------------------
// CMsgMediaInfo::ParseError
// ACCESSOR
// -----------------------------------------------------------------------------
TBool CMsgMediaInfo::ParseError() const
    {
    return iParseError;
    }

// -----------------------------------------------------------------------------
// CMsgMediaInfo::HandlerApplicationUid
// ACCESSOR
// -----------------------------------------------------------------------------
TUid CMsgMediaInfo::HandlerApplicationUid() const
    {
    return iHandlerAppUid;
    }

// -----------------------------------------------------------------------------
// CMsgMediaInfo::HandlerApplicationUid
// MUTATOR
// -----------------------------------------------------------------------------
void CMsgMediaInfo::SetHandlerApplicationUid( TUid aHandlerAppUid )
    {
    iHandlerAppUid = aHandlerAppUid;
    }

