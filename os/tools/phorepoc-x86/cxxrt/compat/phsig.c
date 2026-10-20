/* phsig.c -- OURS (Phorepoc libphcompat). Open C has sigaction/sigemptyset only; the rest of <signal.h> sits behind
   SYMBIAN_OE_POSIX_SIGNALS and is not exported. sigset_t is a 64-bit mask. Signals are never *delivered* to JSC here (see
   browser-plan.md P1: GC thread suspension is cooperative / Thread::suspend is unsupported), so the masks are pure bookkeeping. */
#include <signal.h>
#include <errno.h>
#include <pthread.h>

int sigfillset(sigset_t* s)  { if (!s) { errno = EINVAL; return -1; } *s = ~(sigset_t)0; return 0; }
int sigaddset(sigset_t* s, int n)  { if (!s || n < 1 || n > 63) { errno = EINVAL; return -1; } *s |= ((sigset_t)1 << (n - 1)); return 0; }
int sigdelset(sigset_t* s, int n)  { if (!s || n < 1 || n > 63) { errno = EINVAL; return -1; } *s &= ~((sigset_t)1 << (n - 1)); return 0; }
int sigismember(const sigset_t* s, int n) { if (!s || n < 1 || n > 63) { errno = EINVAL; return -1; } return (int)((*s >> (n - 1)) & 1); }
int pthread_sigmask(int how, const sigset_t* set, sigset_t* old) { (void)how; (void)set; if (old) *old = 0; return 0; }
int sigsuspend(const sigset_t* m) { (void)m; errno = EINTR; return -1; }
int pthread_kill(pthread_t t, int sig) { (void)t; (void)sig; return ENOSYS; }   /* no cross-thread signals: Thread::suspend() reports failure */
