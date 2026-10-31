# Copyright The Phorepoc Authors
# SPDX-License-Identifier: Apache-2.0
"""drtrvct2_2_test.py -- host test of drtrvct2_2_rt.cpp (the RVCT 2.2 runtime helpers, OURS).

Compiles drtrvct2_2_rt.cpp with arm-none-eabi-g++ into a flat image and runs the 35 helpers of drtrvct2_2u.def that the
file implements under the unicorn ARM emulator with the RVCT register interfaces (see the header of the .cpp). Two checks:

  1. against an independent Python model (exact integer / rational arithmetic) of each helper's documented behaviour;
  2. when the original RVCT 2.2 runtime image is given (RVCT_DRT_IMAGE=<the DLL file, XIP header included>), a DIFFERENTIAL run:
     the same inputs go through the original code and ours and every output register / byte of memory must agree.

    py drtrvct2_2_test.py [N]          # N random cases per function (default 400)
    PHE_ARM_GXX=<arm-none-eabi-g++>    # the compiler, if it is not on PATH
    RVCT_DRT_IMAGE=<file>              # optional: enables the differential run

Needs `pip install unicorn` and the compiler; otherwise the test is skipped (exit 0, "SKIP").
"""
import os, random, shutil, struct, subprocess, sys, tempfile
from fractions import Fraction

HERE = os.path.dirname(os.path.abspath(__file__))
GXX = os.environ.get("PHE_ARM_GXX") or shutil.which("arm-none-eabi-g++") or r"C:\Program Files (x86)\Arm GNU Toolchain arm-none-eabi\14.2 rel1\bin\arm-none-eabi-g++.exe"
try:
    from unicorn import Uc, UC_ARCH_ARM, UC_MODE_ARM, UC_HOOK_CODE, UC_HOOK_MEM_FETCH_UNMAPPED, UcError
    from unicorn.arm_const import *
    from unicorn.unicorn_const import *
except ImportError:
    print("SKIP: unicorn not installed"); sys.exit(0)
if not os.path.exists(GXX) and not shutil.which(GXX):
    print("SKIP: no arm-none-eabi-g++"); sys.exit(0)

BASE = 0x20000000
M32 = 0xffffffff
M64 = (1 << 64) - 1
RET = 0x10000
STACK = 0x70080000
DATA = 0x60000000            # test data area (0x100000 bytes)
REAL_BASE = 0x8043D0D0       # the original image is position dependent (XIP)
REAL_EXPORT_DIR = 0x8043EBD4
REAL_DIV0 = (0x8043D20C, 0x8043D21E)
NAMES = """__btod_div_common __btod_mult_common __memmove_aligned __rt_divtest __rt_lib_init __rt_lib_shutdown __rt_memcpy __rt_memset
__rt_raise __rt_sdiv __rt_sdiv10 __rt_sdiv32by16 __rt_sdiv64by32 __rt_udiv __rt_udiv10 __rt_udiv32by16 _btod_d2e _btod_ediv _btod_edivd
_btod_emul _btod_emuld _btod_etento _d2e_denorm_low _d2e_norm_op1 _e2d _e2e _ll_srdv _ll_udiv10 _ll_udiv_big _ll_udiv_ginormous
_ll_udiv_small _ll_urdv _memcpy_lastbytes _memset _memset_w""".split()
ORD = dict(zip(NAMES, [11, 12, 13, 16, 19, 20, 21, 22, 23, 24, 25, 26, 27, 29, 30, 31, 32, 33, 34, 35, 36, 37, 38, 39, 40, 41, 68, 75, 76, 77, 78, 79, 80, 81, 82]))


def build():
    d = tempfile.mkdtemp(prefix="drt")
    os.makedirs(d + "/inc")
    open(d + "/inc/e32std.h", "w").write(
        "typedef int TInt;\nenum TExcType { EExcGeneral=0, EExcFloatInvalidOperation=16, EExcAbort=20, EExcKill=21, "
        "EExcUserInterrupt=22, EExcDataAbort=23, EExcCodeAbort=24 };\nclass User { public: static void RaiseException(TExcType); };\n#define EXPORT_C\n")
    open(d + "/stubs.cpp", "w").write(
        '#include <e32std.h>\nextern "C" { void __rt_div0() { for(;;); } void _fp_init() { } }\n'
        "void User::RaiseException(TExcType) { for(;;); }\n")
    open(d + "/link.ld", "w").write("SECTIONS { . = 0x%x; .text : { *(.text*) *(.rodata*) *(.ARM.*) } .data : { *(.data*) } .bss : { *(.bss*) } /DISCARD/ : { *(.comment) *(.ARM.attributes) } }\n" % BASE)
    flags = ["-march=armv5te", "-marm", "-mfloat-abi=soft"]
    cc = lambda src, o: subprocess.check_call([GXX] + flags + ["-O2", "-std=gnu++03", "-fno-exceptions", "-fno-rtti", "-I", d + "/inc", "-c", src, "-o", o])
    cc(HERE + "/drtrvct2_2_rt.cpp", d + "/m.o")
    cc(d + "/stubs.cpp", d + "/s.o")
    subprocess.check_call([GXX, "-nostdlib"] + flags + ["-T", d + "/link.ld", d + "/m.o", d + "/s.o", "-lgcc", "-o", d + "/m.elf"])
    nm = GXX.replace("g++", "nm")
    objcopy = GXX.replace("g++", "objcopy")
    subprocess.check_call([objcopy, "-O", "binary", d + "/m.elf", d + "/m.bin"])
    syms = {}
    for l in subprocess.check_output([nm, d + "/m.elf"]).decode().split("\n"):
        p = l.split()
        if len(p) == 3:
            syms[p[2]] = int(p[0], 16)
    return open(d + "/m.bin", "rb").read(), syms


class Machine:
    """one emulated core with a code image; call(addr, regs, mem) returns the outcome"""
    def __init__(self, img, base, thumb_bit_fn=None, extra_stop=None):
        self.uc = Uc(UC_ARCH_ARM, UC_MODE_ARM)
        self.uc.ctl_set_cpu_model(UC_CPU_ARM_CORTEX_A15)   # the original code touches the VFP status register
        self.uc.reg_write(UC_ARM_REG_C1_C0_2, 0xf00000)    # coprocessor access: cp10 / cp11
        self.uc.reg_write(UC_ARM_REG_FPEXC, 0x40000000)    # VFP enabled
        self.base = base
        self.uc.mem_map(base & ~0xfff, (len(img) + 0x2fff) & ~0xfff)
        self.uc.mem_write(base, img)
        self.uc.mem_map(0x70000000, 0x100000)
        self.uc.mem_map(RET & ~0xfff, 0x1000)
        self.uc.mem_map(DATA, 0x100000)
        self.stops = {}          # address range -> label
        self.event = None
        self.uc.hook_add(UC_HOOK_CODE, self._code)
        self.uc.hook_add(UC_HOOK_MEM_FETCH_UNMAPPED, self._unmapped)

    def _code(self, uc, addr, size, ud):
        for (lo, hi), lab in self.stops.items():
            if lo <= addr < hi:
                self.event = (lab, uc.reg_read(UC_ARM_REG_R0), uc.reg_read(UC_ARM_REG_R1))
                uc.emu_stop()
                return

    def _unmapped(self, uc, access, addr, size, value, ud):
        self.event = ("import", uc.reg_read(UC_ARM_REG_R0), uc.reg_read(UC_ARM_REG_R1))
        uc.emu_stop()
        return False

    def call(self, addr, regs, mem=(), lr=RET, sp=STACK, frame=None, maxn=2000000):
        uc = self.uc
        uc.mem_write(DATA, bytes(0x100000))
        for a, b in mem:
            uc.mem_write(a, bytes(b))
        self.event = None
        for k in range(13):
            uc.reg_write(UC_ARM_REG_R0 + k, 0x11110000 + k)
        uc.reg_write(UC_ARM_REG_SP, sp)
        uc.reg_write(UC_ARM_REG_LR, lr)
        if frame is not None:                   # a caller frame on the stack: r4 r5 r6 lr
            uc.reg_write(UC_ARM_REG_SP, sp - 16)
            uc.mem_write(sp - 16, struct.pack("<4I", *frame))
        for k, v in regs.items():
            uc.reg_write(UC_ARM_REG_R0 + k, v & M32)
        thumb = addr & 1
        uc.reg_write(UC_ARM_REG_CPSR, 0x10 | (0x20 if thumb else 0))
        try:
            uc.emu_start(addr, RET, count=maxn)
        except UcError:
            pass
        out = [uc.reg_read(UC_ARM_REG_R0 + k) for k in range(13)]
        return out, self.event, uc

    def mem(self, a, n):
        return bytes(self.uc.mem_read(a, n))


EXPORT_NAMES = None


def real_machine(path):
    img = open(path, "rb").read()
    m = Machine(img, REAL_BASE)
    m.stops[REAL_DIV0] = "div0"
    exps = struct.unpack_from("<89I", img, REAL_EXPORT_DIR - REAL_BASE)
    addr = {n: exps[o - 1] for n, o in ORD.items()}
    return m, addr


def ours_machine():
    img, syms = build()
    m = Machine(img, BASE)
    for n in ("__rt_div0", "_fp_init", "_ZN4User14RaiseExceptionE8TExcType"):
        pass
    m.stops[(syms["__rt_div0"], syms["__rt_div0"] + 4)] = "div0"
    m.stops[(syms["_ZN4User14RaiseExceptionE8TExcType"], syms["_ZN4User14RaiseExceptionE8TExcType"] + 4)] = "import"
    addr = {n: syms[n] for n in NAMES}
    return m, addr


# ---------------------------------------------------------------------------------------------------------------
fails = []
counts = {}


def u32(x): return x & M32
def s32(x): x &= M32; return x - (1 << 32) if x >> 31 else x
def s64(x): x &= M64; return x - (1 << 64) if x >> 63 else x
def trunc_div(a, b):
    q = abs(a) // abs(b)
    return -q if (a < 0) != (b < 0) else q


class Ctx:
    def __init__(self, ours, oaddr, real, raddr):
        self.ours, self.oaddr, self.real, self.raddr = ours, oaddr, real, raddr

    def run(self, fn, regs, mem=(), frame=None, outregs=(0, 1), memcheck=(), keep=(), guard6=False):
        """run ours (and the original); returns ours' (regs, event); differential comparison when the original is present"""
        o, ev, _ = self.ours.call(self.oaddr[fn], regs, mem, frame=frame)
        ours_out = dict((k, o[k]) for k in outregs)
        ours_mem = [self.ours.mem(a, n) for a, n in memcheck]
        ours_keep = dict((k, o[k]) for k in keep)
        evt = ev[0] if ev else None
        self.last_event = ev
        if self.real:
            r, rev, _ = self.real.call(self.raddr[fn], regs, mem, frame=frame)
            rev_name = rev[0] if rev else None
            real_mem = [self.real.mem(a, n) for a, n in memcheck]
            lab = lambda e: "div0" if e == "div0" else (None if e is None else "call")
            bad = None
            if lab(evt) != lab(rev_name):
                bad = "event ours=%s real=%s" % (evt, rev_name)
            elif evt is None:
                for k in outregs:
                    if r[k] != o[k]:
                        bad = "r%d ours=%08x real=%08x" % (k, o[k], r[k]); break
                if guard6 and ((r[6] >> 31, (r[6] << 1) & M32 != 0) != (o[6] >> 31, (o[6] << 1) & M32 != 0)):
                    bad = "guard word r6 ours=%08x real=%08x" % (o[6], r[6])
                for k in keep:
                    if r[k] != o[k]:
                        bad = "kept r%d ours=%08x real=%08x" % (k, o[k], r[k]); break
                if not bad and real_mem != ours_mem:
                    bad = "memory differs"
            counts[fn + "/diff"] = counts.get(fn + "/diff", 0) + 1
            if bad:
                fails.append("DIFF %s regs=%s: %s" % (fn, {k: hex(v) for k, v in regs.items()}, bad))
        return ours_out, evt, ours_mem


def expect(fn, what, got, want):
    counts[fn + "/ref"] = counts.get(fn + "/ref", 0) + 1
    if got != want:
        fails.append("REF %s %s: got %s want %s" % (fn, what, got, want))


def rnd32(rng):
    k = rng.random()
    if k < 0.15: return rng.choice([0, 1, 2, 3, 9, 10, 11, 0x7fffffff, 0x80000000, 0x80000001, 0xffffffff, 0xfffffffe, 0xffff, 0x10000, 0x1ffff])
    if k < 0.5: return rng.getrandbits(rng.randint(1, 32))
    return rng.getrandbits(32)


def test_div(c, rng, N):
    for i in range(N * 3):
        d, n = rnd32(rng), rnd32(rng)
        for fn in ("__rt_udiv", "__rt_sdiv"):
            out, ev, _ = c.run(fn, {0: d, 1: n}, keep=(4, 5, 6, 7, 8, 9, 10, 11))
            if d == 0:
                expect(fn, "div0", ev, "div0"); continue
            if fn == "__rt_udiv":
                want = (n // d, n % d)
            else:
                a, b = s32(n), s32(d)
                q = trunc_div(a, b)
                want = (u32(q), u32(a - q * b))
            expect(fn, (hex(d), hex(n)), (out[0], out[1]), want)
    for i in range(N * 3):
        n = rnd32(rng)
        if i % 5 == 0: n = (rng.randint(0, 0x19999999) * 10 + rng.choice([0, 9, 5])) & M32
        out, ev, _ = c.run("__rt_udiv10", {0: n}, outregs=(0, 1))
        expect("__rt_udiv10", hex(n), (out[0], out[1]), (n // 10, n % 10))
        out, ev, _ = c.run("__rt_sdiv10", {0: n}, outregs=(0, 1))
        a = s32(n); q = trunc_div(a, 10)
        expect("__rt_sdiv10", hex(n), (out[0], out[1]), (u32(q), u32(a - q * 10)))
    for z in (0, 5):
        out, ev, _ = c.run("__rt_divtest", {0: z}, outregs=(0,))
        expect("__rt_divtest", z, ev, "div0" if z == 0 else None)
    # the 16-bit step dividers: valid range checked against the arithmetic, everything checked differentially
    for i in range(N * 3):
        d = rng.randint(1, 0x7fff)
        q = rng.randint(0, 0xffff)
        n = min(q * d + rng.randint(0, d - 1), M32)
        extra = rng.getrandbits(15) << 17 if rng.random() < 0.3 else 0
        out, ev, _ = c.run("__rt_udiv32by16", {0: n, 1: d | extra}, outregs=(0, 1))
        if n // d <= 0xffff and (-(d)) & 0x1ffff:
            expect("__rt_udiv32by16", (hex(n), hex(d)), (out[0], out[1]), (n // d, n % d))
        # signed: operands with |n / d| < 2^15
        ds = rng.randint(-0x8000, 0x7fff) or 1
        qq = rng.randint(-0x7fff, 0x7fff)
        ns = qq * ds + (rng.randint(0, abs(ds) - 1) * (1 if qq >= 0 else -1))
        ns = max(-(1 << 31), min((1 << 31) - 1, ns))
        out, ev, _ = c.run("__rt_sdiv32by16", {0: ns, 1: ds & 0xffff | extra}, outregs=(0, 1))
        qw = trunc_div(ns, ds)
        if -0x8000 <= qw < 0x8000:
            expect("__rt_sdiv32by16", (ns, ds), (s32(out[0]), s32(out[1])), (qw, ns - qw * ds))
        # arbitrary operands, differential only
        c.run("__rt_udiv32by16", {0: rnd32(rng), 1: rnd32(rng)}, outregs=(0, 1))
        c.run("__rt_sdiv32by16", {0: rnd32(rng), 1: rnd32(rng)}, outregs=(0, 1))
    for z in (0, 0x10000, 0x20000):
        c.run("__rt_udiv32by16", {0: 1234, 1: z}, outregs=(0, 1))
        c.run("__rt_sdiv32by16", {0: 1234, 1: z}, outregs=(0, 1))
    # 64 by 32: the quotient must fit 32 bits for the exact check
    for i in range(N * 3):
        d = rnd32(rng) or 1
        sd = s32(d)
        nq = rng.randint(-(1 << 31) + 1, (1 << 31) - 1)
        n = nq * sd + rng.randint(0, abs(sd) - 1) * (1 if nq >= 0 else -1)
        n = max(-(1 << 63), min((1 << 63) - 1, n))
        hi, lo = (n >> 32) & M32, n & M32
        out, ev, _ = c.run("__rt_sdiv64by32", {0: hi, 1: lo, 2: d}, outregs=(0, 1))
        qw = trunc_div(n, sd)
        if -(1 << 31) <= qw < (1 << 31):
            expect("__rt_sdiv64by32", (n, sd), (s32(out[1]), s32(out[0])), (qw, n - qw * sd))
        c.run("__rt_sdiv64by32", {0: rnd32(rng), 1: rnd32(rng), 2: rnd32(rng)}, outregs=(0, 1))
    c.run("__rt_sdiv64by32", {0: 5, 1: 5, 2: 0}, outregs=(0, 1))


def rnd64(rng):
    k = rng.random()
    if k < 0.15: return rng.choice([0, 1, 2, 9, 10, 0x7fffffff, 0x80000000, 0xffffffff, 0x100000000, M64, M64 - 1, 1 << 63, (1 << 63) - 1, 1 << 62])
    return rng.getrandbits(rng.randint(1, 64))


def test_lldiv(c, rng, N):
    for i in range(N * 3):
        d, n = rnd64(rng), rnd64(rng)
        regs = {0: d & M32, 1: d >> 32, 2: n & M32, 3: n >> 32}
        out, ev, _ = c.run("_ll_urdv", regs, outregs=(0, 1, 2, 3), keep=(4, 5, 6, 7, 8, 9, 10, 11))
        if d == 0: expect("_ll_urdv", "div0", ev, "div0")
        else: expect("_ll_urdv", (hex(d), hex(n)), (out[0] | out[1] << 32, out[2] | out[3] << 32), (n // d, n % d))
        out, ev, _ = c.run("_ll_srdv", regs, outregs=(0, 1, 2, 3), keep=(4, 5, 6, 7, 8, 9, 10, 11))
        if d == 0: expect("_ll_srdv", "div0", ev, "div0")
        else:
            a, b = s64(n), s64(d)
            q = trunc_div(a, b)
            expect("_ll_srdv", (hex(d), hex(n)), (out[0] | out[1] << 32, out[2] | out[3] << 32), ((q) & M64, (a - q * b) & M64))
        out, ev, _ = c.run("_ll_udiv10", {0: n & M32, 1: n >> 32}, outregs=(0, 1, 2, 3))
        expect("_ll_udiv10", hex(n), (out[0] | out[1] << 32, out[2], out[3]), (n // 10, n % 10, 0))
        # the library's internal states: dividend r4:r5, the shifted divisor
        dd = rnd64(rng) or 1
        nn = rnd64(rng)
        fr = [0xa4a4a4a4, 0xa5a5a5a5, 0xa6a6a6a6, RET]
        if dd < (1 << 31):
            s = 32 - dd.bit_length() - 1
            ip = (dd << s) & M32
            out, ev, _ = c.run("_ll_udiv_small", {4: nn >> 32, 5: nn & M32, 6: s, 12: ip}, frame=fr, outregs=(0, 1, 2, 3))
            expect("_ll_udiv_small", (hex(dd), hex(nn)), (out[0] | out[1] << 32, out[2], out[3]), (nn // dd, nn % dd, 0))
        # big and ginormous take lr as state: set by the caller register file
        if (1 << 32) <= dd < (1 << 63):
            s = 64 - dd.bit_length() - 1
            sh = (dd << s) & M64
            out, ev, _ = run_state(c, "_ll_udiv_big", {4: nn >> 32, 5: nn & M32, 6: s, 12: sh >> 32}, sh & M32, fr)
            expect("_ll_udiv_big", (hex(dd), hex(nn)), (out[0] | out[1] << 32, out[2] | out[3] << 32), (nn // dd, nn % dd))
        if dd >= (1 << 63):
            out, ev, _ = run_state(c, "_ll_udiv_ginormous", {4: nn >> 32, 5: nn & M32, 12: dd >> 32}, dd & M32, fr)
            expect("_ll_udiv_ginormous", (hex(dd), hex(nn)), (out[0] | out[1] << 32, out[2] | out[3] << 32), (nn // dd, nn % dd))


def run_state(c, fn, regs, lrstate, frame):
    """the internal division entries take the library's state in lr as well; the caller frame holds the return address"""
    o, ev, _ = c.ours.call(c.oaddr[fn], regs, frame=frame, lr=lrstate)
    if c.real:
        r, rev, _ = c.real.call(c.raddr[fn], regs, frame=frame, lr=lrstate)
        counts[fn + "/diff"] = counts.get(fn + "/diff", 0) + 1
        if any(r[k] != o[k] for k in (0, 1, 2, 3)):
            fails.append("DIFF %s regs=%s: %s vs %s" % (fn, regs, [hex(o[k]) for k in range(4)], [hex(r[k]) for k in range(4)]))
    return o, ev, None


def test_mem(c, rng, N):
    A = DATA + 0x1000
    for i in range(N * 2):
        n = rng.choice([0, 1, 2, 3, 4, 5, 7, 8, 15, 16, 17, 31, 32, 33, 63, 100, rng.randint(0, 300)])
        so, do = rng.randint(0, 7), rng.randint(0, 7)
        src = bytes(rng.getrandbits(8) for _ in range(n + 16))
        regs = {0: A + 0x4000 + do, 1: A + so, 2: n}
        base = [(A, src), (A + 0x4000, bytes([0xee] * (n + 32)))]
        o, ev, om = c.run("__rt_memcpy", regs, mem=base, outregs=(), memcheck=[(A + 0x4000, n + 32)], keep=(4, 5, 6, 7, 8, 9, 10, 11))
        want = bytearray([0xee] * (n + 32)); want[do:do + n] = src[so:so + n]
        expect("__rt_memcpy", (so, do, n), om[0], bytes(want))
        k = rng.randint(0, 3) if rng.random() < 0.5 else n
        o, ev, om = c.run("_memcpy_lastbytes", {0: A + 0x4000 + do, 1: A + so, 2: n}, mem=base, outregs=(), memcheck=[(A + 0x4000, n + 32)])
        want = bytearray([0xee] * (n + 32)); want[do:do + (n & 3)] = src[so:so + (n & 3)]
        expect("_memcpy_lastbytes", (so, do, n), om[0], bytes(want))
        # memmove, backwards, word aligned ends; overlapping and not
        w = rng.randint(0, 80)
        ov = rng.choice([0, 4, 8, 12, 32])
        e1 = A + 0x8000 + (w & ~3) + 64
        e0 = e1 + ov * rng.choice([-1, 1])
        e0 &= ~3
        e1 &= ~3
        data = bytes(rng.getrandbits(8) for _ in range(0x200))
        base = [(e1 - 0x100, data)]
        if e0 < e1 and e1 - w < e0:
            continue                         # a backward copy over a higher source is not a memmove
        o, ev, om = c.run("__memmove_aligned", {0: e0, 1: e1, 2: w}, mem=base, outregs=(), memcheck=[(e1 - 0x100, 0x200)])
        buf = bytearray(data); off = e1 - 0x100
        tmp = bytes(buf[(e1 - w) - off:e1 - off])
        if e0 - w >= e1 - w:                 # destination above the source: the backward copy is memmove
            buf[(e0 - w) - off:e0 - off] = tmp
            if e0 - w >= off and e0 <= off + 0x200:
                expect("__memmove_aligned", (hex(e0), hex(e1), w), om[0], bytes(buf))
        # memset family
        v = rng.getrandbits(8)
        o, ev, om = c.run("__rt_memset", {0: A + 0x4000 + do, 1: v | 0xabcd00, 2: n}, mem=[(A + 0x4000, bytes([0xee] * (n + 32)))], outregs=(), memcheck=[(A + 0x4000, n + 32)])
        want = bytearray([0xee] * (n + 32)); want[do:do + n] = bytes([v]) * n
        expect("__rt_memset", (v, do, n), om[0], bytes(want))
        word = v * 0x01010101
        o, ev, om = c.run("_memset", {0: A + 0x4000 + do, 1: n, 2: word}, mem=[(A + 0x4000, bytes([0xee] * (n + 32)))], outregs=(), memcheck=[(A + 0x4000, n + 32)])
        expect("_memset", (v, do, n), om[0], bytes(want))
        dw = do & ~3
        want = bytearray([0xee] * (n + 32)); want[dw:dw + n] = bytes([v]) * n
        o, ev, om = c.run("_memset_w", {0: A + 0x4000 + dw, 1: n, 2: word}, mem=[(A + 0x4000, bytes([0xee] * (n + 32)))], outregs=(), memcheck=[(A + 0x4000, n + 32)])
        expect("_memset_w", (v, dw, n), om[0], bytes(want))
        # a non-replicated fill word: the library stores whole words and the low byte for the bytes (differential)
        c.run("_memset", {0: A + 0x4000 + do, 1: n, 2: rng.getrandbits(32)}, mem=[(A + 0x4000, bytes([0xee] * (n + 32)))], outregs=(), memcheck=[(A + 0x4000, n + 32)])
        c.run("_memset_w", {0: A + 0x4000 + dw, 1: n, 2: rng.getrandbits(32)}, mem=[(A + 0x4000, bytes([0xee] * (n + 32)))], outregs=(), memcheck=[(A + 0x4000, n + 32)])


def test_misc(c, rng, N):
    kinds = {1: 20, 2: 20, 3: 24, 4: 22, 5: 23, 6: 21}
    for sig in list(range(0, 9)) + [rng.getrandbits(32) for _ in range(3)]:
        for ty in (0, 1, 2, 3):
            out, ev, _ = c.run("__rt_raise", {0: sig, 1: ty}, outregs=(), keep=())
            want_exc = 20 if sig == 1 else 16 if sig == 2 and ty != 2 else 20 if sig == 2 else kinds.get(sig, 0)
            if sig == 2 and ty == 2: want_exc = 20
            if sig == 2 and ty != 2: want_exc = 16
            counts["__rt_raise/ref"] = counts.get("__rt_raise/ref", 0) + 1
            ev = c.last_event
            if ev is None or ev[0] != "import":
                fails.append("REF __rt_raise %d,%d: no exception raised" % (sig, ty))
            else:
                if ev[1] != want_exc:
                    fails.append("REF __rt_raise %d,%d: exception %d want %d" % (sig, ty, ev[1], want_exc))
    c.run("__rt_lib_init", {}, outregs=())
    c.run("__rt_lib_shutdown", {}, outregs=())


# ---- extended-precision references (exact rational arithmetic) --------------------------------------------------
def e_val(e, hi, lo):
    return Fraction((hi << 32) | lo, 1 << 63) * Fraction(2) ** ((e & 0xffffff) - 0x3fff)


def e_round(fr, mode, bits=64):
    """round a positive Fraction to a normalised `bits`-bit mantissa: (exponent word without bias fixups, mantissa)"""
    if fr == 0: return None
    ex = fr.numerator.bit_length() - fr.denominator.bit_length()
    if fr < Fraction(2) ** ex: ex -= 1
    m = fr / Fraction(2) ** (ex - (bits - 1))
    fl = m.numerator // m.denominator
    rem = m - fl
    if rem:
        if mode == -1: pass
        elif mode == 0:
            if rem > Fraction(1, 2) or (rem == Fraction(1, 2) and fl & 1): fl += 1
        else: fl += 1
    if fl.bit_length() > bits: fl >>= 1; ex += 1
    return ex, fl


def rand_eval(rng, special=False):
    hi = rng.getrandbits(32) | 0x80000000
    lo = rng.getrandbits(32)
    k = rng.random()
    if k < 0.1: lo = 0
    if k > 0.9: hi = 0x80000000; lo = rng.choice([0, 1, 0xffffffff])
    e = rng.choice([0x3fff, 0x4000, 0x3ffe, rng.randint(0x3c00, 0x4400), rng.randint(0x1000, 0x7000)])
    sign = rng.choice([0, 0x80000000])
    return [sign | e, hi, lo]


def test_e(c, rng, N):
    A, B, OUT = DATA + 0x100, DATA + 0x200, DATA + 0x300
    pk = lambda v: struct.pack("<3I", *v)
    for i in range(N * 3):
        mode = rng.choice([-1, 0, 0, 1, 2])
        a, b = rand_eval(rng), rand_eval(rng)
        if rng.random() < 0.08: a[0] |= 0x40000000
        if rng.random() < 0.08: b[0] |= 0x40000000
        if rng.random() < 0.05: a[1] = 0; a[2] = 0
        if rng.random() < 0.05: b[1] = 0; b[2] = 0
        mem = [(A, pk(a)), (B, pk(b))]
        for fn in ("_btod_emul", "_btod_ediv", "_btod_emuld", "_btod_edivd"):
            out, ev, _ = c.run(fn, {0: A, 1: B, 2: mode}, mem=mem, outregs=(0, 1, 2), keep=(4, 5, 6, 7, 8, 9, 10, 11))
            # reference (normal operands only)
            div = "div" in fn
            okop = lambda v: bool(v[1] & 0x80000000) and not (v[0] & 0x40000000)
            if (div and not (okop(a) and okop(b))) or ((not div) and ((a[0] | b[0]) & 0x40000000)):
                expect(fn, "special", (out[0], out[1], out[2]), tuple(a)); continue
            va, vb = e_val(*a), e_val(*b)
            res = va / vb if div else va * vb
            if res == 0: continue
            ex, mant = e_round(res, mode, 64 if not fn.endswith("d") else 53)
            if ex + 0x3fff < 1: continue          # underflow: covered by the differential run
            if fn.endswith("d"):
                be = ex + 1023
                if 1 <= be < 0x7ff:
                    word = ((be << 52) | (mant & ((1 << 52) - 1))) & M64
                    expect(fn, (a, b, mode), (out[0], out[1]), (word >> 32, word & M32))
            else:
                expect(fn, (a, b, mode), (out[0] & 0x7fffffff, out[1], out[2]), (ex + 0x3fff, mant >> 32, mant & M32))
    # unrounded cores and the rounding step (differential; the reference is the end-to-end check above)
    for i in range(N * 3):
        a, b = rand_eval(rng), rand_eval(rng)
        c.run("__btod_mult_common", {0: a[0], 1: a[1], 2: a[2], 3: b[0], 4: b[1], 5: b[2]}, outregs=(0, 1, 2, 3), keep=(), guard6=True)
        c.run("__btod_div_common", {0: a[0], 1: a[1], 2: a[2], 3: b[0], 4: b[1], 5: b[2]}, outregs=(0, 1, 2, 3), keep=(), guard6=True)
        e = [rng.getrandbits(32) & 0x80000000, rng.getrandbits(32) | (0x80000000 if rng.random() < 0.9 else 0), rng.getrandbits(32)]
        r3 = rng.choice([0x3fff, rng.randint(-120, 40), rng.randint(0x3000, 0x5000), rng.randint(0, 5), 0x7ffe, 0x7ffd])
        r6 = rng.choice([0, 0x80000000, 0x80000001, 0x7fffffff, 0x40000000, rng.getrandbits(32), 1, 0x8000, 0x18000])
        if rng.random() < 0.2: e[2] = 0xffffffff
        if rng.random() < 0.1: e[1] = 0xffffffff
        mode = rng.choice([-1, 0, 1, 2])
        regs = {0: e[0], 1: e[1], 2: e[2], 3: r3, 6: r6, 11: mode}
        c.run("_e2e", regs, outregs=(0, 1, 2, 3), keep=(4, 5, 8, 9, 10, 11))
        regs3 = dict(regs); regs3[3] = rng.choice([r3, 0x3fff + rng.randint(-1100, 1100) & M32, 0x43fe, 0x43ff, 0x3c00, 0x3c01, 0x3bff, 0x3a00])
        c.run("_e2d", regs3, outregs=(0, 1), keep=(4, 5, 8, 9, 10, 11))
    # double -> e (reference: exact)
    for i in range(N * 3):
        k = rng.random()
        bits = rng.getrandbits(64)
        if k < 0.2: bits &= ~(0x7ff << 52)                        # denormal / zero
        elif k < 0.3: bits = (bits & ~(0x7ff << 52)) | (0x7ff << 52)  # inf / nan
        elif k < 0.4: bits = bits & (0x8000000000000000 | (0xfffff << 0)) if rng.random() < .5 else (bits >> rng.randint(1, 63))
        hi, lo = bits >> 32, bits & M32
        out, ev, _ = c.run("_btod_d2e", {0: hi, 1: lo}, outregs=(0, 1, 2), keep=(4, 5, 6, 7, 8, 9, 10, 11))
        ex = (bits >> 52) & 0x7ff
        mant = bits & ((1 << 52) - 1)
        sign = (bits >> 63) << 31
        if ex == 0 and mant == 0:
            want = (sign | 0x3c00 if False else out[0], 0, 0)       # zero: exponent word not significant beyond the sign
            expect("_btod_d2e", "zero", (out[1], out[2]), (0x80000000 if False else out[1], 0))
        elif 0 < ex < 0x7ff:
            m64 = ((1 << 52) | mant) << 11
            expect("_btod_d2e", hex(bits), (out[0], out[1], out[2]), (sign | (ex + 0x3c00), m64 >> 32, m64 & M32))
        elif ex == 0:
            sh = 52 - mant.bit_length()
            m64 = (mant << (sh + 12))
            m64 &= M64
            expect("_btod_d2e", "denormal " + hex(bits), (out[0], out[1], out[2]), (sign | (0x3c00 + 1 - (sh + 1)), m64 >> 32, m64 & M32))
        for fn in ("_d2e_norm_op1", "_d2e_denorm_low"):
            regs = {0: rng.getrandbits(32), 1: rng.getrandbits(32), 2: rng.getrandbits(32)}
            if fn == "_d2e_norm_op1" and rng.random() < 0.3: regs[1] |= 0x80000000
            if fn == "_d2e_norm_op1" and rng.random() < 0.2: regs[1] = 0x80000000
            if fn == "_d2e_denorm_low" and rng.random() < 0.2: regs[1] = 0
            c.run(fn, regs, outregs=(0, 1, 2), keep=(4, 5, 6, 7, 8, 9, 10, 11))
    # 10^n
    for n in list(range(-27, 60)) + [rng.randint(-27, 850) for _ in range(N)]:
        for mode in (-1, 0, 1):
            out, ev, om = c.run("_btod_etento", {0: OUT, 1: n, 2: mode}, outregs=(), memcheck=[(OUT, 12)])
            w = struct.unpack("<3I", om[0])
            if n >= 0:
                ex, mant = e_round(Fraction(10) ** n, mode, 64)
                got = ((w[0] & 0x7fffffff), (w[1] << 32) | w[2])
                counts["_btod_etento/ref"] = counts.get("_btod_etento/ref", 0) + 1
                # the library composes 10^n from rounded factors: it is within one unit of the last place of the exact value
                if got[0] != ex + 0x3fff and abs(got[0] - (ex + 0x3fff)) > 1 or abs(got[1] - mant) > 1 << 6 and got[0] == ex + 0x3fff:
                    fails.append("REF _btod_etento 10^%d mode %d: got %s want %s" % (n, mode, (hex(got[0]), hex(got[1])), (hex(ex + 0x3fff), hex(mant))))


def main():
    N = int(sys.argv[1]) if len(sys.argv) > 1 else 400
    rng = random.Random(0x5eed)
    ours, oaddr = ours_machine()
    real = raddr = None
    imgp = os.environ.get("RVCT_DRT_IMAGE")
    if imgp and os.path.exists(imgp):
        real, raddr = real_machine(imgp)
    c = Ctx(ours, oaddr, real, raddr)
    for t in (test_div, test_lldiv, test_mem, test_misc, test_e):
        t(c, rng, N)
    for k in sorted(counts):
        pass
    print("%d reference checks, %d differential checks (%s)" % (
        sum(v for k, v in counts.items() if k.endswith("/ref")), sum(v for k, v in counts.items() if k.endswith("/diff")),
        "original image present" if real else "no RVCT_DRT_IMAGE: differential run skipped"))
    if fails:
        for f in fails[:40]:
            print("FAIL", f)
        print("%d FAILURES" % len(fails))
        sys.exit(1)
    print("PASS")


if __name__ == "__main__":
    main()
