// cxxrt_main.cpp -- Symbian side of the C++17 runtime test: main() runs the standard-C++ test and reports over RDebug (serial log).
#include <e32std.h>
#include <e32debug.h>
extern "C" int cxx17_test_run(char* aMsg, int aLen);
extern "C" void ph_log(const char* aMsg) { RDebug::Printf("[cxxrt] %s", aMsg); }
extern "C" void ph_logx(const char* aMsg, unsigned a, unsigned b) { RDebug::Printf("[cxxrt] %s: %x %x", aMsg, a, b); }
int main()
	{
	ph_log("main entered");
	char msg[128];
	int fails = cxx17_test_run(msg, sizeof msg);
	RDebug::Printf("[cxxrt] %s fails=%d", msg, fails);
	return fails;
	}
