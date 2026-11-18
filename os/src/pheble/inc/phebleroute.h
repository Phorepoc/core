// Copyright The Phorepoc Authors
// SPDX-License-Identifier: EPL-2.0 OR GPL-2.0-or-later
//
// phebleroute.h -- OURS (BLE plan rung 3): the seam between the HCI router (pheblerouter.dll, an HCTL plug-in that
// wraps the board's real HCTL) and the LE host (pheblehost.dll: NimBLE + the PheBle server). Internal to pheble.
//
// Packets cross this seam WITHOUT transport framing: a command is opcode(2, LE) len(1) params; an event is code(1)
// len(1) params; ACL is handle+flags(2) len(2) data. Each call copies; the callee never keeps the descriptor.

#ifndef PHEBLEROUTE_H
#define PHEBLEROUTE_H

#include <e32base.h>

/** The router side, as the LE host sees it. Callable from any thread of the process (the router marshals onto the
    Bluetooth stack's thread, where the wrapped HCTL lives). */
class MPheBleHciSink
	{
public:
	/** An HCI command for the controller. Completion arrives as an event through MPheBleHost::PbhEvent. */
	virtual void PbsCommand(const TDesC8& aCommand) = 0;
	/** An ACL packet of an LE link for the controller. */
	virtual void PbsAcl(const TDesC8& aAcl) = 0;
	};

/** The LE host, as the router sees it. Called on the Bluetooth stack's thread; must not block. */
class MPheBleHost
	{
public:
	/** An event that belongs to the LE host (LE meta, the completion of its own commands, events of LE handles). */
	virtual void PbhEvent(const TDesC8& aEvent) = 0;
	/** ACL data from the controller on an LE connection handle. */
	virtual void PbhAcl(const TDesC8& aAcl) = 0;
	/** ETrue once the BR/EDR stack has initialised the controller (the LE host may sync); EFalse on power off,
	    hard reset or a transport failure (the LE host resets, its connections are gone). */
	virtual void PbhControllerReady(TBool aReady) = 0;
	};

/** pheblehost.dll ordinal 1: attach a router and get the LE host. The host DLL pins itself in memory on the first
    attach (its NimBLE threads and static data outlive a router that is unloaded when the stack stops). */
IMPORT_C MPheBleHost* PheBleHostAttach(MPheBleHciSink* aRouter);
/** pheblehost.dll ordinal 2: the router is going away; the host stops sending to it. */
IMPORT_C void PheBleHostDetach(MPheBleHciSink* aRouter);

#endif // PHEBLEROUTE_H
