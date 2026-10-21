// Copyright The Phorepoc Authors
// SPDX-License-Identifier: Apache-2.0

// pheclient.cpp -- OURS: RPheTty, the client side of phettyd (see phetty.h)
#include "phetty.h"

TInt RPheTty::StartServer()
	{
	RProcess p;
	TInt r = p.Create(KPheTtyServerExe, _L("force"));
	if (r != KErrNone)
		return r;
	TRequestStatus st;
	p.Rendezvous(st);
	if (st != KRequestPending)
		{
		p.Kill(0);                       // rendezvous refused: the process is dead already
		}
	else
		{
		p.Resume();
		}
	User::WaitForRequest(st);
	TInt v = (p.ExitType() == EExitPanic) ? KErrGeneral : st.Int();
	p.Close();
	return v;
	}

TInt RPheTty::ConnectNoStart()
	{
	return CreateSession(KPheTtyServerName, TVersion(1, 0, 0), 4);
	}

TInt RPheTty::Connect()
	{
	TInt r = ConnectNoStart();
	if (r == KErrNotFound)
		{
		r = StartServer();
		if (r == KErrNone || r == KErrAlreadyExists)
			r = ConnectNoStart();
		}
	return r;
	}

TInt RPheTty::New(TInt aCols, TInt aRows)
	{ return SendReceive(EPtNew, TIpcArgs(aCols, aRows)); }

TInt RPheTty::List(TUint8* aIds, TInt aMax)
	{
	TPtr8 d(aIds, 0, aMax);
	return SendReceive(EPtList, TIpcArgs(&d));
	}

TInt RPheTty::Info(TInt aId, TPheTtyInfo& aInfo)
	{
	TPckg<TPheTtyInfo> p(aInfo);
	return SendReceive(EPtInfo, TIpcArgs(aId, &p));
	}

TInt RPheTty::Resize(TInt aId, TInt aCols, TInt aRows)
	{ return SendReceive(EPtResize, TIpcArgs(aId, aCols, aRows)); }

TInt RPheTty::Kill(TInt aId)
	{ return SendReceive(EPtKill, TIpcArgs(aId)); }

TInt RPheTty::Key(TInt aId, TUint aCode, TUint aMods)
	{ return SendReceive(EPtKey, TIpcArgs(aId, aCode, aMods)); }

TInt RPheTty::Bytes(TInt aId, const TDesC8& aBytes)
	{ return SendReceive(EPtBytes, TIpcArgs(aId, &aBytes)); }

void RPheTty::Read(TInt aId, TDes8& aBuf, TUint32 aFrom, TBool aWait, TRequestStatus& aStatus)
	{ SendReceive(EPtRead, TIpcArgs(aId, &aBuf, (TInt)aFrom, aWait ? 1 : 0), aStatus); }

void RPheTty::ReadCancel()
	{ SendReceive(EPtReadCancel); }

TInt RPheTty::ConWrite(TInt aId, const TDesC8& aBytes)
	{ return SendReceive(EPtConWrite, TIpcArgs(aId, &aBytes)); }

void RPheTty::ConRead(TInt aId, TPheKeyBuf& aKey, TRequestStatus& aStatus)
	{ SendReceive(EPtConRead, TIpcArgs(aId, &aKey), aStatus); }

void RPheTty::ConReadCancel()
	{ SendReceive(EPtConReadCancel); }

TInt RPheTty::ConSize(TInt aId)
	{ return SendReceive(EPtConSize, TIpcArgs(aId)); }

TInt RPheTty::ConTitle(TInt aId, const TDesC& aTitle)
	{ return SendReceive(EPtConTitle, TIpcArgs(aId, &aTitle)); }

TInt RPheTty::ShellState(TInt aId)
	{ return SendReceive(EPtShellState, TIpcArgs(aId)); }

TInt RPheTty::Adopt(TInt aId, TUint aPid)
	{ return SendReceive(EPtAdopt, TIpcArgs(aId, (TInt)aPid)); }

TInt RPheTty::TtyOf(TUint aPid)
	{ return SendReceive(EPtTtyOf, TIpcArgs((TInt)aPid)); }
