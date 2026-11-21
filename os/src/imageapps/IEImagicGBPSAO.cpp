/*
* Copyright (c) 2009 Nokia Corporation and/or its subsidiary(-ies).
* All rights reserved.
* This component and the accompanying materials are made available
* under the terms of "Eclipse Public License v1.0"
* which accompanies this distribution, and is available
* at the URL "http://www.eclipse.org/legal/epl-v10.html".
*
* Initial Contributors:
* Nokia Corporation - initial contribution.
*
* Contributors: Juha Kauppinen, Mika Hokkanen
* 
* Description: Photo Browser
*
*/

#include "IEImagicBGPSAO.h"
#include "debug.h"

CImagicBGPSAO* CImagicBGPSAO::NewL(MIETNInternalObserver& aObserver)
{
	CImagicBGPSAO* self = new (ELeave) CImagicBGPSAO(aObserver);
	CleanupStack::PushL(self);
	self->ConstructL();
	CleanupStack::Pop();
	return self;
}

CImagicBGPSAO::~CImagicBGPSAO()
{
    DP0_IMAGIC((_L("CIETNGeneratorAO::SetImageArray --")));
    DeActivateTNCreatoAO();
    DP0_IMAGIC((_L("CIETNGeneratorAO::SetImageArray --")));
}

//EPriorityIdle, EPriorityLow, EPriorityStandard, EPriorityUserInput, EPriorityHigh
CImagicBGPSAO::CImagicBGPSAO(MIETNInternalObserver& aObserver)
: CActive(EPriorityStandard),   // PHOREPOC PATCH: was EPriorityIdle; the event that the thumbnail server has finished one picture was only seen when nothing else in the app was ready, which a drawing loop never allows
iIETNObserver(aObserver)
{
    
}

void CImagicBGPSAO::ConstructL()
{
	CActiveScheduler::Add(this);
}

void CImagicBGPSAO::RunL()
{
	TInt error = iStatus.Int();
	iIETNObserver.HandleEvents((TBGPSEventCode)iStatus.Int());
}

void CImagicBGPSAO::DoCancel()
{	
}

TInt CImagicBGPSAO::RunError(TInt /*aError*/)
{
	return KErrNone;
}

void CImagicBGPSAO::ActivateTNCreatorAO()
{
	if(!IsActive())
		SetActive();
}

void CImagicBGPSAO::DeActivateTNCreatoAO()
{
    DP0_IMAGIC((_L("CIETNGeneratorAO::DeActivateTNCreatoAO ++")));
	if(IsActive())
		Cancel();
	DP0_IMAGIC((_L("CIETNGeneratorAO::DeActivateTNCreatoAO --")));
}

 
