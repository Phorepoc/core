/* phmisc.c -- OURS (Phorepoc libphcompat): small POSIX/glibc names WTF/JSC call that Open C lacks. */
#include <stddef.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <unistd.h>
#include <time.h>
#include <fcntl.h>
#include <sys/types.h>

long long llabs(long long v) { return v < 0 ? -v : v; }

void* memmem(const void* h, size_t hl, const void* n, size_t nl)
{
    const char* p = (const char*)h;
    if (nl == 0) return (void*)h;
    if (hl < nl) return NULL;
    for (size_t i = 0; i + nl <= hl; ++i)
        if (p[i] == ((const char*)n)[0] && memcmp(p + i, n, nl) == 0) return (void*)(p + i);
    return NULL;
}

unsigned long long strtoull(const char* s, char** end, int base)
{
    unsigned long long v = 0; const char* p = s; int any = 0;
    while (*p == ' ' || (*p >= 9 && *p <= 13)) ++p;
    if (*p == '+') ++p;
    if ((base == 0 || base == 16) && p[0] == '0' && (p[1] == 'x' || p[1] == 'X')) { p += 2; base = 16; }
    else if (base == 0) base = (p[0] == '0') ? 8 : 10;
    for (;; ++p) {
        int d = (*p >= '0' && *p <= '9') ? *p - '0' : (*p >= 'a' && *p <= 'z') ? *p - 'a' + 10 : (*p >= 'A' && *p <= 'Z') ? *p - 'A' + 10 : 99;
        if (d >= base) break;
        v = v * (unsigned)base + (unsigned)d; any = 1;
    }
    if (end) *end = (char*)(any ? p : s);
    return v;
}

ssize_t pread(int fd, void* buf, size_t n, off_t off)
{
    off_t cur = lseek(fd, 0, SEEK_CUR);
    if (cur < 0 || lseek(fd, off, SEEK_SET) < 0) return -1;
    ssize_t r = read(fd, buf, n);
    lseek(fd, cur, SEEK_SET);
    return r;
}

int mkostemp(char* tmpl, int flags) { (void)flags; return mkstemp(tmpl); }
int madvise(void* a, size_t l, int adv) { (void)a; (void)l; (void)adv; return 0; }
int prctl(int op, ...) { (void)op; return 0; }
int getrlimit(int r, void* lim) { (void)r; if (lim) memset(lim, 0xff, 2 * sizeof(long)); return 0; }
int getrusage(int who, void* ru) { (void)who; if (ru) memset(ru, 0, 72); return 0; }
int fallocate(int fd, int mode, long long off, long long len) { (void)fd; (void)mode; (void)off; (void)len; errno = ENOSYS; return -1; }
int dladdr(const void* a, void* info) { (void)a; (void)info; return 0; }

/* aligned allocation: Open C's malloc has none. The block is over-allocated and the original pointer is kept in front, BUT free() cannot
   undo that -> only for callers that never free (WTF's tryReserveUncommittedAligned) or free through ph_aligned_free. */
void* ph_aligned_malloc(size_t align, size_t size)
{
    char* raw = (char*)malloc(size + align + sizeof(void*));
    if (!raw) return NULL;
    char* p = (char*)(((size_t)raw + sizeof(void*) + align - 1) & ~(align - 1));
    ((void**)p)[-1] = raw;
    return p;
}
void ph_aligned_free(void* p) { if (p) free(((void**)p)[-1]); }

/* ---- thread ids, filesystem, misc ---- */
#include <pthread.h>
#include <sys/stat.h>
#include <sys/statvfs.h>
#include <sys/syscall.h>
#include <stdarg.h>

long syscall(long no, ...)            /* only SYS_gettid is ever asked for (WTF thread ids) */
{
    if (no == SYS_gettid) return (long)pthread_self();
    errno = ENOSYS; return -1;
}
int statvfs(const char* path, struct statvfs* st) { (void)path; (void)st; errno = ENOSYS; return -1; }
int pthread_attr_setstack(void* a, void* addr, size_t sz) { (void)a; (void)addr; (void)sz; return ENOSYS; }  /* Open C threads own their stacks */

char* mkdtemp(char* tmpl)
{
    size_t n = strlen(tmpl); unsigned seed = (unsigned)time(NULL) ^ (unsigned)(size_t)tmpl;
    int tries;
    if (n < 6 || strcmp(tmpl + n - 6, "XXXXXX")) { errno = EINVAL; return NULL; }
    for (tries = 0; tries < 100; ++tries) {
        int i;
        for (i = 0; i < 6; ++i) { seed = seed * 1103515245u + 12345u; tmpl[n - 6 + i] = "abcdefghijklmnopqrstuvwxyz0123456789"[(seed >> 16) % 36]; }
        if (mkdir(tmpl, 0700) == 0) return tmpl;
        if (errno != EEXIST) return NULL;
    }
    return NULL;
}

/* randomness: euser's Math::Random (shim phsym.cpp) */
extern unsigned ph_random32(void);
unsigned arc4random(void) { return ph_random32(); }
extern void ph_random_bytes(void* buf, unsigned n);   /* the kernel CSPRNG in bulk (phsym.cpp) */
void arc4random_buf(void* buf, size_t n) { ph_random_bytes(buf, (unsigned)n); }

