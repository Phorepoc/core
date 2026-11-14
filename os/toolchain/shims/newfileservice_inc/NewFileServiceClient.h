/* Copyright The Phorepoc Authors
 * SPDX-License-Identifier: EPL-2.0 OR GPL-2.0-or-later */

// OURS: NewFileServiceClient.h -- the S60 "new file service" client (embed Camera / Voice Recorder to
// capture a new media file). Its package is not in the 2010 open-source drop; messaging's
// MsgAttachmentUtils.cpp (msgeditormodel) uses it to insert a freshly captured image/video/clip.
// Declarations as in the public S60 SDK header. The factory lives in newservice.dll, which does not
// exist here: its import is a link stub, so FetchNewL panics if reached (camera/recorder capture).
#ifndef NEWFILESERVICECLIENT_H
#define NEWFILESERVICECLIENT_H

#include <e32base.h>
#include <badesca.h>
#include <NewFileService.hrh>

class CAiwGenericParamList;
class RFile;

class CNewFileServiceClient : public CBase
    {
public:
    virtual TBool NewFileL( TUid aApplicationUid, CDesCArray& aFileNames,
                            CAiwGenericParamList* aParams, TNewServiceFileType aFileType,
                            TBool aMultipleFiles ) = 0;
    virtual TBool NewFileL( TUid aApplicationUid, RFile& aFileHandle,
                            CAiwGenericParamList* aParams, TNewServiceFileType aFileType ) = 0;
    };

class NewFileServiceFactory
    {
public:
    IMPORT_C static CNewFileServiceClient* NewClientL();
    };

#endif
