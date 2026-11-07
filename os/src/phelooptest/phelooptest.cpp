// Copyright The Phorepoc Authors
// SPDX-License-Identifier: Apache-2.0

// phelooptest.cpp -- OURS: boot test of Open C sockets on the loopback with NO connection up (no RConnection, no setdefaultif, no IAP):
// a TCP listen / connect / accept / send / recv exchange and a UDP datagram to itself, all on 127.0.0.1. The 2010 backend refused every
// socket call with ECONNABORTED in this state (its socket is opened lazily and needs a connection); the Phorepoc backend opens it without
// one and reopens it through the connection preferences only when it is used for another address. The last check shows that other
// addresses keep the old rule (bind on every address needs an interface: errno 53 here when no connection can be started; informational).
#include <e32std.h>
#include <e32debug.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>

static int gOk, gFail;
#define CHECK(c, what) do { errno = 0; if (c) { gOk++; RDebug::Printf("[phelooptest] ok: %s", what); } else { gFail++; RDebug::Printf("[phelooptest] FAIL: %s (errno %d)", what, errno); } } while (0)

static void Tcp()
	{
	int one = 1, ls = socket(AF_INET, SOCK_STREAM, 0);
	CHECK(ls >= 0, "socket(stream)");
	CHECK(setsockopt(ls, SOL_SOCKET, SO_REUSEADDR, &one, sizeof one) == 0, "setsockopt(SO_REUSEADDR) with no connection");
	sockaddr_in a; memset(&a, 0, sizeof a); a.sin_family = AF_INET; a.sin_addr.s_addr = htonl(INADDR_LOOPBACK); a.sin_port = 0;
	CHECK(bind(ls, (sockaddr*)&a, sizeof a) == 0, "bind(127.0.0.1:0)");
	socklen_t len = sizeof a;
	CHECK(getsockname(ls, (sockaddr*)&a, &len) == 0 && a.sin_port != 0, "getsockname gives a port");
	RDebug::Printf("[phelooptest] listening on 127.0.0.1:%d", ntohs(a.sin_port));
	{ char serv[16] = {0}; int r = getnameinfo((sockaddr*)&a, len, NULL, 0, serv, sizeof serv, NI_NUMERICHOST | NI_NUMERICSERV);
	  RDebug::Printf("[phelooptest] getnameinfo(getsockname, len %d) -> %d (%s), service %s", (int)len, r, r ? gai_strerror(r) : "ok", serv);
	  CHECK(r == 0 && atoi(serv) == ntohs(a.sin_port), "getnameinfo of the getsockname address gives the port"); }
	CHECK(listen(ls, 4) == 0, "listen");
	int cs = socket(AF_INET, SOCK_STREAM, 0);
	{ errno = 0; int r = setsockopt(cs, IPPROTO_TCP, 1 /* TCP_NODELAY */, &one, sizeof one); RDebug::Printf("[phelooptest] info: setsockopt(TCP_NODELAY) -> %d errno %d", r, errno); }
	CHECK(connect(cs, (sockaddr*)&a, sizeof a) == 0, "connect(127.0.0.1)");
	int as = accept(ls, 0, 0);
	CHECK(as >= 0, "accept");
	char buf[16] = {0};
	errno = 0; int n = (int)send(cs, "ping", 4, 0);
	RDebug::Printf("[phelooptest] send(client) -> %d errno %d", n, errno);
	CHECK(n == 4, "send client -> server");
	errno = 0; n = (int)recv(as, buf, sizeof buf, 0);
	RDebug::Printf("[phelooptest] recv(server) -> %d errno %d", n, errno);
	CHECK(n == 4 && !memcmp(buf, "ping", 4), "recv at the server");
	errno = 0; n = (int)send(as, "pong!", 5, 0);
	RDebug::Printf("[phelooptest] send(server) -> %d errno %d", n, errno);
	CHECK(n == 5, "send server -> client");
	memset(buf, 0, sizeof buf);
	errno = 0; n = (int)recv(cs, buf, sizeof buf, 0);
	RDebug::Printf("[phelooptest] recv(client) -> %d errno %d", n, errno);
	CHECK(n == 5 && !memcmp(buf, "pong!", 5), "recv at the client");
	close(as); close(cs); close(ls);
	}

static void Udp()
	{
	int s = socket(AF_INET, SOCK_DGRAM, 0);
	sockaddr_in a; memset(&a, 0, sizeof a); a.sin_family = AF_INET; a.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
	CHECK(bind(s, (sockaddr*)&a, sizeof a) == 0, "udp bind(127.0.0.1:0)");
	socklen_t len = sizeof a;
	getsockname(s, (sockaddr*)&a, &len);
	CHECK(sendto(s, "dgram", 5, 0, (sockaddr*)&a, sizeof a) == 5, "udp sendto self");
	char buf[16] = {0};
	CHECK(recvfrom(s, buf, sizeof buf, 0, 0, 0) == 5 && !memcmp(buf, "dgram", 5), "udp recvfrom");
	close(s);
	}

static void Other()
	{
	int one = 1, s = socket(AF_INET, SOCK_STREAM, 0);
	setsockopt(s, SOL_SOCKET, SO_REUSEADDR, &one, sizeof one);
	sockaddr_in a; memset(&a, 0, sizeof a); a.sin_family = AF_INET; a.sin_addr.s_addr = htonl(INADDR_ANY);
	int r = bind(s, (sockaddr*)&a, sizeof a);
	RDebug::Printf("[phelooptest] info: bind(any) with no connection -> %d, errno %d (53 = the 2010 rule: needs a connection)", r, r ? errno : 0);
	close(s);
	}

// getaddrinfo with no connection: a numeric address and "localhost" must resolve without one (libdbus asks AF_UNSPEC + AI_ADDRCONFIG | AI_PASSIVE)
static void Resolve()
	{
	static const struct { const char* host; int family; int flags; } t[] = {
		{ "127.0.0.1", AF_UNSPEC, AI_ADDRCONFIG | AI_PASSIVE },
		{ "127.0.0.1", AF_UNSPEC, AI_PASSIVE },
		{ "127.0.0.1", AF_INET, AI_PASSIVE },
		{ "127.0.0.1", AF_INET, AI_NUMERICHOST },
		{ "localhost", AF_INET, 0 },
		{ "localhost", AF_UNSPEC, AI_ADDRCONFIG },
		{ "::1", AF_UNSPEC, AI_NUMERICHOST } };
	for (unsigned i = 0; i < sizeof t / sizeof t[0]; i++)
		{
		struct addrinfo hints, *res = 0;
		memset(&hints, 0, sizeof hints); hints.ai_family = t[i].family; hints.ai_flags = t[i].flags; hints.ai_socktype = SOCK_STREAM;
		int r = getaddrinfo(t[i].host, "0", &hints, &res);
		RDebug::Printf("[phelooptest] getaddrinfo(%s, family %d, flags 0x%x) -> %d (%s)", t[i].host, t[i].family, t[i].flags, r, r ? gai_strerror(r) : "ok");
		if (res) freeaddrinfo(res);
		}
	}

GLDEF_C TInt E32Main()
	{
	RDebug::Printf("[phelooptest] start: loopback with no connection");
	Tcp();
	Udp();
	Other();
	Resolve();
	RDebug::Printf("[phelooptest] done: %d passed, %d failed", gOk, gFail);
	return gFail ? 1 : 0;
	}
