// Copyright The Phorepoc Authors
// SPDX-License-Identifier: Apache-2.0
//
// npl_symbian.cpp -- OURS: the NimBLE porting layer (NPL) on Symbian EKA2.
//
// NimBLE runs in its own threads inside the Bluetooth stack's process (c32exe, where the HCI router plug-in loads
// pheblehost.dll). The NPL contract (nimble/include/nimble/nimble_npl.h, NimBLE 1.9.0) maps as follows:
//   - the "critical section"      = one process-wide RMutex (nestable for its holder, like the Linux port's lock);
//   - event queues                = an intrusive list under that lock + an RSemaphore signalled once per put;
//   - mutexes / semaphores        = RMutex / RSemaphore created EOwnerProcess (any thread of the process may use them);
//   - callouts (timers)           = one timer thread: a sorted active list, an RSemaphore wait with the timeout of the
//                                   nearest expiry, then ble_npl_eventq_put of the callout's event on its queue;
//   - time                        = milliseconds from the nanokernel tick count (HAL ENanoTickPeriod);
//   - tasks                       = RThread with a C trampoline.
// Written from NimBLE's own port contract and its Linux/FreeRTOS NPL as the reference of the semantics.

#include <e32base.h>
#include <e32debug.h>
#include <hal.h>
#include <stdio.h>
#include <stdarg.h>

extern "C" {
#include "nimble/nimble_npl.h"
#include "nimble/nimble_port.h"
}

namespace {

RMutex gLock;                      // the NPL critical section
TBool gInited = EFalse;
TInt gTickUs = 1000;               // nanokernel tick period, microseconds

// timer thread state (all under gLock)
ble_npl_callout* gActive = NULL;   // sorted by c_ticks
RSemaphore gTimerKick;
TInt gTaskCount = 0;

inline void Lock() { gLock.Wait(); }
inline void Unlock() { gLock.Signal(); }

RSemaphore Sem(int aHandle) { RSemaphore s; s.SetHandle(aHandle); return s; }
RMutex Mtx(int aHandle) { RMutex m; m.SetHandle(aHandle); return m; }

TInt TicksToUs(ble_npl_time_t aTicks)
	{
	if (aTicks >= (ble_npl_time_t)(KMaxTInt / 1000))
		return KMaxTInt;
	return (TInt)aTicks * 1000;
	}

void TimerUnlink(ble_npl_callout* aCo)
	{
	for (ble_npl_callout** p = &gActive; *p; p = &(*p)->c_next)
		{
		if (*p == aCo)
			{
			*p = aCo->c_next;
			aCo->c_next = NULL;
			return;
			}
		}
	}

TInt TimerThread(TAny*)
	{
	for (;;)
		{
		TInt waitUs = KMaxTInt;
		Lock();
		ble_npl_time_t now = ble_npl_time_get();
		while (gActive && (ble_npl_stime_t)(gActive->c_ticks - now) <= 0)
			{
			ble_npl_callout* co = gActive;
			gActive = co->c_next;
			co->c_next = NULL;
			co->c_active = false;
			if (co->c_evq)
				ble_npl_eventq_put(co->c_evq, &co->c_ev);
			else
				ble_npl_event_run(&co->c_ev);   // no queue: run in the timer thread (NimBLE does not do this today)
			}
		if (gActive)
			waitUs = TicksToUs(gActive->c_ticks - now);
		Unlock();
		if (waitUs == KMaxTInt)
			gTimerKick.Wait();
		else
			gTimerKick.Wait(waitUs > 0 ? waitUs : 1);
		}
	}

struct TTaskStart
	{
	ble_npl_task_func_t iFunc;
	void* iArg;
	};

TInt TaskTrampoline(TAny* aStart)
	{
	TTaskStart start = *static_cast<TTaskStart*>(aStart);
	User::Free(aStart);
	CTrapCleanup* cleanup = CTrapCleanup::New();
	start.iFunc(start.iArg);
	delete cleanup;
	return KErrNone;
	}

} // namespace

extern "C" {

int phe_npl_init(void)
	{
	if (gInited)
		return 0;
	TInt err = gLock.CreateLocal(EOwnerProcess);
	if (err == KErrNone)
		err = gTimerKick.CreateLocal(0, EOwnerProcess);
	TInt tick = 0;
	if (err == KErrNone && HAL::Get(HALData::ENanoTickPeriod, tick) == KErrNone && tick > 0)
		gTickUs = tick;
	if (err == KErrNone)
		{
		RThread t;
		err = t.Create(_L("PheBleTimer"), TimerThread, 0x2000, NULL, NULL, EOwnerProcess);
		if (err == KErrNone)
			{
			t.SetPriority(EPriorityMore);
			t.Resume();
			t.Close();
			}
		}
	if (err == KErrNone)
		gInited = ETrue;
	return err;
	}

void phe_nimble_vlog(int lvl, const char *fmt, va_list args)
	{
	char buf[200];
	vsnprintf(buf, sizeof(buf), fmt, args);
	TPtrC8 p((const TUint8*)buf);
	// NimBLE's messages carry their own newlines; RDebug adds one
	TInt len = p.Length();
	while (len > 0 && (p[len - 1] == '\n' || p[len - 1] == '\r'))
		--len;
	TPtrC8 line = p.Left(len);
	RDebug::Printf("nimble[%d] %S", lvl, &line);
	}

bool ble_npl_os_started(void)
	{
	return gInited;
	}

void* ble_npl_get_current_task_id(void)
	{
	return (void*)(TUint)RThread().Id();
	}

// ---- event queues ----

struct ble_npl_eventq* ble_npl_eventq_dflt_get(void)
	{
	return nimble_port_get_dflt_eventq();
	}

void ble_npl_eventq_init(struct ble_npl_eventq *evq)
	{
	RSemaphore s;
	s.CreateLocal(0, EOwnerProcess);
	evq->head = evq->tail = NULL;
	evq->sem = s.Handle();
	}

bool ble_npl_eventq_is_empty(struct ble_npl_eventq *evq)
	{
	Lock();
	bool empty = evq->head == NULL;
	Unlock();
	return empty;
	}

void ble_npl_eventq_put(struct ble_npl_eventq *evq, struct ble_npl_event *ev)
	{
	Lock();
	if (ev->ev_queued)
		{
		Unlock();
		return;
		}
	ev->ev_queued = 1;
	ev->ev_next = NULL;
	if (evq->tail)
		evq->tail->ev_next = ev;
	else
		evq->head = ev;
	evq->tail = ev;
	Unlock();
	Sem(evq->sem).Signal();
	}

struct ble_npl_event* ble_npl_eventq_get(struct ble_npl_eventq *evq, ble_npl_time_t tmo)
	{
	for (;;)
		{
		Lock();
		ble_npl_event* ev = evq->head;
		if (ev)
			{
			evq->head = ev->ev_next;
			if (!evq->head)
				evq->tail = NULL;
			ev->ev_next = NULL;
			ev->ev_queued = 0;
			}
		Unlock();
		if (ev)
			return ev;
		if (tmo == 0)
			return NULL;
		if (tmo == BLE_NPL_TIME_FOREVER)
			Sem(evq->sem).Wait();
		else if (Sem(evq->sem).Wait(TicksToUs(tmo)) != KErrNone)
			return NULL;
		// a signal may belong to an event another getter took or that was removed: loop
		}
	}

void ble_npl_eventq_run(struct ble_npl_eventq *evq)
	{
	ble_npl_event* ev = ble_npl_eventq_get(evq, BLE_NPL_TIME_FOREVER);
	if (ev)
		ble_npl_event_run(ev);
	}

void ble_npl_eventq_remove(struct ble_npl_eventq *evq, struct ble_npl_event *ev)
	{
	Lock();
	if (ev->ev_queued)
		{
		ble_npl_event* prev = NULL;
		for (ble_npl_event* e = evq->head; e; prev = e, e = e->ev_next)
			{
			if (e == ev)
				{
				if (prev)
					prev->ev_next = e->ev_next;
				else
					evq->head = e->ev_next;
				if (evq->tail == e)
					evq->tail = prev;
				break;
				}
			}
		ev->ev_queued = 0;
		ev->ev_next = NULL;
		}
	Unlock();
	}

void ble_npl_event_init(struct ble_npl_event *ev, ble_npl_event_fn *fn, void *arg)
	{
	memset(ev, 0, sizeof(*ev));
	ev->ev_cb = fn;
	ev->ev_arg = arg;
	}

bool ble_npl_event_is_queued(struct ble_npl_event *ev)
	{
	return ev->ev_queued != 0;
	}

void* ble_npl_event_get_arg(struct ble_npl_event *ev)
	{
	return ev->ev_arg;
	}

void ble_npl_event_set_arg(struct ble_npl_event *ev, void *arg)
	{
	ev->ev_arg = arg;
	}

void ble_npl_event_run(struct ble_npl_event *ev)
	{
	ev->ev_cb(ev);
	}

// ---- mutexes ----

ble_npl_error_t ble_npl_mutex_init(struct ble_npl_mutex *mu)
	{
	if (!mu)
		return BLE_NPL_INVALID_PARAM;
	RMutex m;
	if (m.CreateLocal(EOwnerProcess) != KErrNone)
		return BLE_NPL_ENOMEM;
	mu->handle = m.Handle();
	return BLE_NPL_OK;
	}

ble_npl_error_t ble_npl_mutex_pend(struct ble_npl_mutex *mu, ble_npl_time_t timeout)
	{
	if (!mu)
		return BLE_NPL_INVALID_PARAM;
	RMutex m = Mtx(mu->handle);
	if (timeout == BLE_NPL_TIME_FOREVER)
		{
		m.Wait();
		return BLE_NPL_OK;
		}
	if (timeout == 0)
		return m.Poll() == KErrNone ? BLE_NPL_OK : BLE_NPL_TIMEOUT;
	return m.Wait(TicksToUs(timeout)) == KErrNone ? BLE_NPL_OK : BLE_NPL_TIMEOUT;
	}

ble_npl_error_t ble_npl_mutex_release(struct ble_npl_mutex *mu)
	{
	if (!mu)
		return BLE_NPL_INVALID_PARAM;
	Mtx(mu->handle).Signal();
	return BLE_NPL_OK;
	}

// ---- semaphores ----

ble_npl_error_t ble_npl_sem_init(struct ble_npl_sem *sem, uint16_t tokens)
	{
	if (!sem)
		return BLE_NPL_INVALID_PARAM;
	RSemaphore s;
	if (s.CreateLocal(tokens, EOwnerProcess) != KErrNone)
		return BLE_NPL_ENOMEM;
	sem->handle = s.Handle();
	sem->count = tokens;
	return BLE_NPL_OK;
	}

ble_npl_error_t ble_npl_sem_pend(struct ble_npl_sem *sem, ble_npl_time_t timeout)
	{
	if (!sem)
		return BLE_NPL_INVALID_PARAM;
	RSemaphore s = Sem(sem->handle);
	TInt err;
	if (timeout == BLE_NPL_TIME_FOREVER)
		{
		s.Wait();
		err = KErrNone;
		}
	else if (timeout == 0)
		err = s.Poll();
	else
		err = s.Wait(TicksToUs(timeout));
	if (err != KErrNone)
		return BLE_NPL_TIMEOUT;
	Lock();
	--sem->count;
	Unlock();
	return BLE_NPL_OK;
	}

ble_npl_error_t ble_npl_sem_release(struct ble_npl_sem *sem)
	{
	if (!sem)
		return BLE_NPL_INVALID_PARAM;
	Lock();
	++sem->count;
	Unlock();
	Sem(sem->handle).Signal();
	return BLE_NPL_OK;
	}

uint16_t ble_npl_sem_get_count(struct ble_npl_sem *sem)
	{
	return sem->count > 0 ? (uint16_t)sem->count : 0;
	}

// ---- callouts ----

void ble_npl_callout_init(struct ble_npl_callout *c, struct ble_npl_eventq *evq, ble_npl_event_fn *ev_cb, void *ev_arg)
	{
	memset(c, 0, sizeof(*c));
	ble_npl_event_init(&c->c_ev, ev_cb, ev_arg);
	c->c_evq = evq;
	}

ble_npl_error_t ble_npl_callout_reset(struct ble_npl_callout *c, ble_npl_time_t ticks)
	{
	if (ticks == 0)
		ticks = 1;
	Lock();
	TimerUnlink(c);
	c->c_ticks = ble_npl_time_get() + ticks;
	c->c_active = true;
	ble_npl_callout** p = &gActive;
	while (*p && (ble_npl_stime_t)((*p)->c_ticks - c->c_ticks) <= 0)
		p = &(*p)->c_next;
	c->c_next = *p;
	*p = c;
	Unlock();
	gTimerKick.Signal();
	return BLE_NPL_OK;
	}

void ble_npl_callout_stop(struct ble_npl_callout *c)
	{
	Lock();
	TimerUnlink(c);
	c->c_active = false;
	Unlock();
	}

bool ble_npl_callout_is_active(struct ble_npl_callout *c)
	{
	return c->c_active;
	}

ble_npl_time_t ble_npl_callout_get_ticks(struct ble_npl_callout *co)
	{
	return co->c_ticks;
	}

ble_npl_time_t ble_npl_callout_remaining_ticks(struct ble_npl_callout *co, ble_npl_time_t now)
	{
	ble_npl_stime_t rt = (ble_npl_stime_t)(co->c_ticks - now);
	return rt > 0 ? (ble_npl_time_t)rt : 0;
	}

void ble_npl_callout_set_arg(struct ble_npl_callout *co, void *arg)
	{
	co->c_ev.ev_arg = arg;
	}

// ---- time ----

ble_npl_time_t ble_npl_time_get(void)
	{
	// 64-bit intermediate: NTickCount * tick period wraps at 2^32 ms like the Linux port's clock
	return (ble_npl_time_t)(((TUint64)User::NTickCount() * (TUint64)gTickUs) / 1000);
	}

ble_npl_error_t ble_npl_time_ms_to_ticks(uint32_t ms, ble_npl_time_t *out_ticks)
	{
	*out_ticks = ms;
	return BLE_NPL_OK;
	}

ble_npl_error_t ble_npl_time_ticks_to_ms(ble_npl_time_t ticks, uint32_t *out_ms)
	{
	*out_ms = ticks;
	return BLE_NPL_OK;
	}

ble_npl_time_t ble_npl_time_ms_to_ticks32(uint32_t ms)
	{
	return ms;
	}

uint32_t ble_npl_time_ticks_to_ms32(ble_npl_time_t ticks)
	{
	return ticks;
	}

void ble_npl_time_delay(ble_npl_time_t ticks)
	{
	User::AfterHighRes(TicksToUs(ticks));
	}

// ---- critical section ----

uint32_t ble_npl_hw_enter_critical(void)
	{
	Lock();
	return 0;
	}

void ble_npl_hw_exit_critical(uint32_t ctx)
	{
	(void)ctx;
	Unlock();
	}

bool ble_npl_hw_is_in_critical(void)
	{
	return gLock.IsHeld();
	}

void ble_npl_hw_set_isr(int irqn, void (*addr)(void))
	{
	(void)irqn;
	(void)addr;
	}

// ---- tasks ----

int ble_npl_task_init(struct ble_npl_task *t, const char *name, ble_npl_task_func_t func, void *arg, uint8_t prio,
		ble_npl_time_t sanity_itvl, ble_npl_stack_t *stack_bottom, uint16_t stack_size)
	{
	(void)prio;
	(void)sanity_itvl;
	(void)stack_bottom;
	TTaskStart* start = static_cast<TTaskStart*>(User::Alloc(sizeof(TTaskStart)));
	if (!start)
		return BLE_NPL_ENOMEM;
	start->iFunc = func;
	start->iArg = arg;
	TBuf<32> tname;
	tname.Copy(TPtrC8((const TUint8*)(name ? name : "nimble")).Left(32));
	RThread th;
	TInt stack = stack_size > 0x4000 ? stack_size : 0x4000;
	TInt err = th.Create(tname, TaskTrampoline, stack, NULL, start, EOwnerProcess);
	if (err != KErrNone)
		{
		User::Free(start);
		return BLE_NPL_ERROR;
		}
	t->handle = th.Handle();
	t->name = name;
	Lock();
	++gTaskCount;
	Unlock();
	th.Resume();
	return BLE_NPL_OK;
	}

int ble_npl_task_remove(struct ble_npl_task *t)
	{
	RThread th;
	th.SetHandle(t->handle);
	th.Kill(KErrNone);
	th.Close();
	Lock();
	--gTaskCount;
	Unlock();
	return BLE_NPL_OK;
	}

uint8_t ble_npl_task_count(void)
	{
	return (uint8_t)gTaskCount;
	}

void ble_npl_task_yield(void)
	{
	User::AfterHighRes(0);
	}

} // extern "C"
