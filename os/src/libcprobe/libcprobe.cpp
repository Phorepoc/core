// libcprobe.cpp -- OURS: probe of the from-source P.I.P.S. stack (libc -> backend, libm, libpthread).
// A native E32Main exe (no mcrt0): each step prints its result to the debug port, so a fault
// names the layer -- formatted output (varargs + gdtoa), heap, qsort, stdio on C:, time, libm, pthreads.
#include <e32std.h>
#include <e32debug.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <time.h>
#include <math.h>
#include <pthread.h>

static int CmpInt(const void* a, const void* b)
	{
	return *(const int*)a - *(const int*)b;
	}

static void* ThreadFn(void* aArg)
	{
	*(int*)aArg = 4242;
	return aArg;
	}

GLDEF_C TInt E32Main()
	{
	RDebug::Printf("[libcprobe] start");
	char buf[128];
	int n = snprintf(buf, sizeof(buf), "%d %s %5.3f %x %lld", -17, "str", 3.14159, 0xBEEF, 1234567890123LL);
	RDebug::Printf("[libcprobe] snprintf(%d) = '%s'", n, buf);

	double d = strtod("2.5e3", NULL);
	RDebug::Printf("[libcprobe] strtod -> %d   atoi -> %d", (int)d, atoi("  -99x"));

	char* p = (char*)malloc(1000);
	strcpy(p, "heap ok");
	p = (char*)realloc(p, 100000);
	RDebug::Printf("[libcprobe] malloc/realloc -> %s", p ? p : "NULL");
	free(p);

	int v[6] = { 5, -2, 9, 0, 7, 1 };
	qsort(v, 6, sizeof(int), CmpInt);
	RDebug::Printf("[libcprobe] qsort -> %d %d %d %d %d %d", v[0], v[1], v[2], v[3], v[4], v[5]);

	volatile double two = 2.0;							// volatile: keep g++ from folding the libm calls away
	RDebug::Printf("[libcprobe] sqrt(2)*1e6 -> %d   pow(2,10) -> %d   sin(pi/2)*1000 -> %d",
				   (int)(sqrt(two) * 1e6), (int)pow(two, 10.0), (int)(sin(M_PI / two) * 1000));

	FILE* f = fopen("c:\libcprobe.txt", "w");
	RDebug::Printf("[libcprobe] fopen(w) -> %s errno=%d", f ? "ok" : "NULL", errno);
	if (f)
		{
		int w = fprintf(f, "line %d\n", 1);
		fclose(f);
		f = fopen("c:\libcprobe.txt", "r");
		char line[32] = "";
		char* g = f ? fgets(line, sizeof(line), f) : NULL;
		if (g && strchr(line, '\n'))
			*strchr(line, '\n') = 0;
		RDebug::Printf("[libcprobe] fprintf -> %d   fgets -> '%s'", w, g ? line : "NULL");
		if (f)
			fclose(f);
		}

	time_t t = time(NULL);
	RDebug::Printf("[libcprobe] time -> %d", (int)t);

	pthread_t th;
	int out = 0;
	void* ret = NULL;
	int r = pthread_create(&th, NULL, ThreadFn, &out);
	if (r == 0)
		r = pthread_join(th, &ret);
	RDebug::Printf("[libcprobe] pthread create/join -> %d   value %d   ret ok %d", r, out, ret == &out);

	RDebug::Printf("[libcprobe] done");
	return 0;
	}
