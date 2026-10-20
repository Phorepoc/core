// cxx17_test.cpp -- P0 spike (browser-plan.md): a modern C++ runtime (libstdc++ 13) over Open C on Path C (x32).
#include <string>
#include <vector>
#include <map>
#include <memory>
#include <functional>
#include <algorithm>
#include <stdexcept>
#include <atomic>
#include <mutex>
#include <thread>
#include <optional>
#include <variant>
#include <string_view>
#include <cstdio>
#include <cstring>
static char g_msg[128];
extern "C" void ph_log(const char*);
#define ph_printf(...) std::snprintf(g_msg + std::strlen(g_msg), sizeof g_msg - std::strlen(g_msg), __VA_ARGS__)

struct Shape { virtual ~Shape() = default; virtual int area() const = 0; };
struct Sq : Shape { int s; explicit Sq(int s) : s(s) {} int area() const override { return s * s; } };

static int fails = 0;
#define CHECK(c) do { if (!(c)) { ph_printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #c); ++fails; } } while (0)

extern "C" int cxx17_test_run(char* aMsg, int aLen)
{
	ph_log("stage 1: strings/containers");
	std::string s = "hello";
	s += ", c++17 on symbian";
	CHECK(s.size() == 23);
	std::vector<int> v{5, 3, 9, 1};
	std::sort(v.begin(), v.end());
	CHECK(v.front() == 1 && v.back() == 9);
	std::map<std::string, int> m{{"a", 1}, {"b", 2}};
	CHECK(m["b"] == 2);
	auto p = std::make_unique<Sq>(7);
	std::unique_ptr<Shape> sh = std::move(p);
	CHECK(sh->area() == 49);
	std::function<int(int)> f = [&](int x) { return x + (int)v.size(); };
	CHECK(f(1) == 5);
	std::optional<int> o = 4; CHECK(*o == 4);
	std::variant<int, std::string> var = std::string("x"); CHECK(std::get<std::string>(var) == "x");
	std::string_view sv(s); CHECK(sv.substr(0, 5) == "hello");
#ifdef PH_EH   // C++ exceptions: NOT needed by WebKit (it builds -fno-exceptions -fno-rtti); see browser-plan.md P0 results
	ph_log("stage 2: exceptions");
	bool caught = false;
	try { throw std::runtime_error("boom"); }
	catch (const std::exception& e) { caught = std::string(e.what()) == "boom"; }
	CHECK(caught);
	caught = false;
	try { (void)v.at(99); } catch (const std::out_of_range&) { caught = true; }
	CHECK(caught);
#endif
	ph_log("stage 3: threads");
	std::mutex mu; std::atomic<int> cnt{0}; int plain = 0;
	std::vector<std::thread> ts;
	for (int i = 0; i < 4; i++)
		ts.emplace_back([&] { for (int k = 0; k < 1000; k++) { std::lock_guard<std::mutex> g(mu); ++plain; ++cnt; } });
	for (auto& t : ts) t.join();
	CHECK(plain == 4000 && cnt == 4000);
	ph_log("done");
	ph_printf("cxx17_test: %s (%d fails)\n", fails ? "FAIL" : "PASS", fails);
	std::strncpy(aMsg, g_msg, aLen - 1); aMsg[aLen - 1] = 0;
	return fails;
}
