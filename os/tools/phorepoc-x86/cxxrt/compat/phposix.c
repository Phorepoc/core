/* phposix.c -- OURS (Phorepoc, libphcompat): POSIX/BSD calls the WebKit page stack references that Open C lacks (census_web.sh,
 * browser-plan.md section 9). Each is either the real thing on top of what Open C/the kernel has, or a documented "not available". */
#include <errno.h>
#include <math.h>
#include <stddef.h>
#include <sys/types.h>

extern void ph_random_bytes(void* buf, unsigned n);   /* the kernel CSPRNG (phsym.cpp) */
extern void* ph_memalign(size_t align, size_t size);   /* phmem.c: aligned + free()-able */

/* getentropy(2): up to 256 bytes from the kernel CSPRNG */
int getentropy(void* buf, size_t len)
{
    if (len > 256) {
        errno = EIO;
        return -1;
    }
    ph_random_bytes(buf, (unsigned)len);
    return 0;
}

int isblank(int c)
{
    return c == ' ' || c == '\t';
}

void* memalign(size_t align, size_t size)
{
    return ph_memalign(align, size);
}

void sincosf(float x, float* s, float* c)   /* volatile: else GCC folds sinf+cosf back into sincosf (see phmath.c sincos) */
{
    volatile float y = x;
    *s = sinf(y);
    *c = cosf(x);
}

/* POSIX shared memory: no named shm objects on Open C (WebCore's SharedMemory is the multi-process path; unused in-process) */
int shm_open(const char* name, int oflag, mode_t mode)
{
    (void)name; (void)oflag; (void)mode;
    errno = ENOSYS;
    return -1;
}

int shm_unlink(const char* name)
{
    (void)name;
    errno = ENOSYS;
    return -1;
}

/* socketpair: Open C has no AF_UNIX socketpair; curl's multi wakeup and WebKit's IPC fall back when it fails */
int socketpair(int domain, int type, int protocol, int sv[2])
{
    (void)domain; (void)type; (void)protocol; (void)sv;
    errno = EOPNOTSUPP;
    return -1;
}
