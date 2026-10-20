// eh_register.cpp -- C++ exceptions in a Path C (x32) E32 image: the link script (x32_exe_eh.ld) keeps .eh_frame inside the code
// segment between __eh_frame_start and a zero terminator; this static constructor hands it to libgcc's unwinder
// (unwind-dw2-fde: registered objects are searched BEFORE dl_iterate_phdr). dl_iterate_phdr itself (no program headers in an
// E32 image) is a stub that finds nothing.
extern "C" {
extern char __eh_frame_start[];
struct ph_eh_object { void* a; void* b; void* c; void* d; void* e[8]; };
void __register_frame_info(const void*, ph_eh_object*);
int dl_iterate_phdr(int (*)(void*, unsigned, void*), void*) { return 0; }
}
static ph_eh_object g_eh_obj;
extern "C" void ph_log(const char*);
extern "C" void ph_logx(const char*, unsigned, unsigned);
__attribute__((constructor(101))) static void ph_eh_register()
	{
	const unsigned* p = (const unsigned*)__eh_frame_start;
	unsigned n = 0, bad = 0;
	while (*p && n < 100000)
		{
		if (p[1]) { const unsigned* cie = (const unsigned*)((const char*)(p + 1) - p[1]); if (cie[1]) ++bad; }
		p = (const unsigned*)((const char*)p + p[0] + 4); ++n;
		}
	ph_logx("eh_frame start / entries", (unsigned)(unsigned long)__eh_frame_start, n);
	ph_logx("eh_frame bad FDEs / first word", bad, *(const unsigned*)__eh_frame_start);
	__register_frame_info(__eh_frame_start, &g_eh_obj);
	ph_log("eh_frame registered");
	}
// libgcc_eh (Ubuntu build, glibc >= 2.35) falls back to these when a pc is not in the registered .eh_frame: "not found".
extern "C" int _dl_find_object(void*, void*) { return -1; }
// libgcc's gthr "is libpthread linked" probe (weak glibc symbol): present => the unwinder takes its registry mutex.
extern "C" int pthread_key_create(unsigned*, void (*)(void*));
extern "C" int __pthread_key_create(unsigned* k, void (*d)(void*)) { return pthread_key_create(k, d); }
