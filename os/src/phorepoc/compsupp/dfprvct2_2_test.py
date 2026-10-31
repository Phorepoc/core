# Copyright The Phorepoc Authors
# SPDX-License-Identifier: Apache-2.0
"""dfprvct2_2_test.py -- host test of dfprvct2_2.cpp (the RVCT 2.2 floating-point runtime, OURS).

Compiles dfprvct2_2.cpp with arm-none-eabi-g++ (soft-float, as the ROM build does) into a flat image, runs every one of
its 40 exports under the unicorn ARM emulator with the RVCT register interface (double = r0:r1, second operand r2:r3, float
= r0 / r1, long long = r0:r1; the comparisons return their result in the CPSR flags) and compares with an independent
Python reference of the IEEE 754 behaviour (round to nearest even; NaN results compare as "any NaN").

    py dfprvct2_2_test.py [N]          # N random cases per function (default 1500)
    PHE_ARM_GXX=<arm-none-eabi-g++>    # the compiler, if it is not on PATH

Needs `pip install unicorn` and the compiler; otherwise the test is skipped (exit 0, "SKIP").
"""
import math, os, random, shutil, struct, subprocess, sys, tempfile
from fractions import Fraction

HERE = os.path.dirname(os.path.abspath(__file__))
GXX = os.environ.get("PHE_ARM_GXX") or shutil.which("arm-none-eabi-g++") or r"C:\Program Files (x86)\Arm GNU Toolchain arm-none-eabi\14.2 rel1\bin\arm-none-eabi-g++.exe"
try:
    from unicorn import Uc, UC_ARCH_ARM, UC_MODE_ARM, UC_HOOK_CODE
    from unicorn.arm_const import *
except ImportError:
    print("SKIP: unicorn not installed"); sys.exit(0)
if not os.path.exists(GXX) and not shutil.which(GXX):
    print("SKIP: no arm-none-eabi-g++"); sys.exit(0)

EXPORTS = """__dcmp4 __fcmp4 _dabs _dcmp4 _dcmpge _dfix_r _dfixu_r _dflt _dfltu _dneq _drdiv _drem _drnd _dsqrt _fabs _fcmp4 _fcmpge
_ffix_r _ffixu_r _fflt _ffltu _fneq _frdiv _frem _frnd _fsqrt _ll_sfrom_d_r _ll_sfrom_f_r _ll_ufrom_d_r _ll_ufrom_f_r __ARM_scalbn
_ll_sfrom_d _ll_sfrom_f _ll_sto_d _ll_sto_f _ll_ufrom_d _ll_ufrom_f _ll_uto_d _ll_uto_f __ARM_scalbnf""".split()
BASE = 0x20000000
M64 = (1 << 64) - 1


def build():
    d = tempfile.mkdtemp(prefix="dfp")
    os.makedirs(d + "/inc")
    open(d + "/inc/e32std.h", "w").write("#define EXPORT_C\n")
    open(d + "/link.ld", "w").write("SECTIONS { . = 0x%x; .text : { *(.text*) *(.rodata*) *(.ARM.*) } .data : { *(.data*) } .bss : { *(.bss*) } /DISCARD/ : { *(.comment) *(.ARM.attributes) } }\n" % BASE)
    flags = ["-mcpu=cortex-a7", "-marm", "-mfloat-abi=soft"]
    subprocess.check_call([GXX] + flags + ["-O2", "-std=gnu++03", "-fno-exceptions", "-fno-rtti", "-I", d + "/inc", "-c", HERE + "/dfprvct2_2.cpp", "-o", d + "/m.o"])
    subprocess.check_call([GXX, "-nostdlib"] + flags + ["-T", d + "/link.ld", d + "/m.o", "-lgcc", "-o", d + "/m.elf"])
    nm = GXX.replace("g++", "nm")
    objcopy = GXX.replace("g++", "objcopy")
    subprocess.check_call([objcopy, "-O", "binary", d + "/m.elf", d + "/m.bin"])
    syms = {}
    for l in subprocess.check_output([nm, d + "/m.elf"]).decode().split("\n"):
        p = l.split()
        if len(p) == 3:
            syms[p[2]] = int(p[0], 16)
    return open(d + "/m.bin", "rb").read(), syms


class Cpu:
    def __init__(self, img):
        self.uc = Uc(UC_ARCH_ARM, UC_MODE_ARM)
        self.uc.mem_map(BASE, (len(img) + 0x2fff) & ~0xfff)
        self.uc.mem_write(BASE, img)
        self.uc.mem_map(0x70000000, 0x100000)
        self.uc.mem_map(0x10000, 0x1000)

    def call(self, addr, regs):
        uc = self.uc
        uc.reg_write(UC_ARM_REG_CPSR, 0x10)
        uc.reg_write(UC_ARM_REG_SP, 0x70080000)
        uc.reg_write(UC_ARM_REG_LR, 0x10000)
        for k in range(4, 13):
            uc.reg_write(UC_ARM_REG_R0 + k, 0x11110000 + k)
        for i, v in enumerate(regs):
            uc.reg_write(UC_ARM_REG_R0 + i, v & 0xffffffff)
        uc.emu_start(addr, 0x10000, count=200000)
        out = [uc.reg_read(UC_ARM_REG_R0 + i) for i in range(4)]
        pres = [uc.reg_read(UC_ARM_REG_R0 + k) for k in range(4, 13)]
        return out, uc.reg_read(UC_ARM_REG_CPSR) >> 28, pres, uc.reg_read(UC_ARM_REG_SP)


def dbl(b): return struct.unpack("<d", struct.pack("<Q", b))[0]
def dbits(x): return struct.unpack("<Q", struct.pack("<d", x))[0]
def flt(b): return struct.unpack("<f", struct.pack("<I", b))[0]
def isnand(b): return ((b << 1) & M64) > 0xFFE0000000000000
def isnanf(b): return ((b << 1) & 0xffffffff) > 0xFF000000
def regs_d(b): return [b & 0xffffffff, b >> 32]


def round_fraction(fr, p, emin, emax, bias, ebits):
    """exact Fraction -> IEEE bits (p-bit significand incl. the hidden bit), nearest even"""
    sign = 1 if fr < 0 else 0
    fr = abs(fr)
    if fr == 0:
        return sign << (p - 1 + ebits)
    e = fr.numerator.bit_length() - fr.denominator.bit_length()
    if Fraction(2) ** e > fr:
        e -= 1
    e = max(e, emin)
    q = fr / Fraction(2) ** (e - (p - 1))
    n = q.numerator // q.denominator
    rem = q - n
    if rem > Fraction(1, 2) or (rem == Fraction(1, 2) and n & 1):
        n += 1
    if n >= 1 << p:
        n >>= 1
        e += 1
    if e > emax:
        return (sign << (p - 1 + ebits)) | (((1 << ebits) - 1) << (p - 1))
    if n < 1 << (p - 1):
        be = 0
    else:
        be = e + bias
    return (sign << (p - 1 + ebits)) | (be << (p - 1)) | (n & ((1 << (p - 1)) - 1))


def to_d(fr): return round_fraction(fr, 53, -1022, 1023, 1023, 11)
def to_f(fr): return round_fraction(fr, 24, -126, 127, 127, 8)


def exact(bits, isd):
    """finite IEEE bits -> Fraction"""
    return Fraction(dbl(bits) if isd else flt(bits))


def rint_even(fr):
    n = fr.numerator // fr.denominator
    rem = fr - n
    if rem > Fraction(1, 2) or (rem == Fraction(1, 2) and n & 1):
        n += 1
    return n


def trunc(fr): return int(fr)   # Fraction.__int__ truncates toward zero


def sat_i(v, bits):
    lo, hi = -(1 << (bits - 1)), (1 << (bits - 1)) - 1
    return max(lo, min(hi, v)) & ((1 << bits) - 1)


def sat_u(v, bits): return max(0, min((1 << bits) - 1, v))


def to_int(bits, isd, rounding, signed, nbits):
    if (isnand(bits) if isd else isnanf(bits)):
        return 0
    inf = (bits & 0x7fffffffffffffff) == 0x7ff0000000000000 if isd else (bits & 0x7fffffff) == 0x7f800000
    if inf:
        neg = bits >> (63 if isd else 31)
        v = -(1 << 70) if neg else 1 << 70
    else:
        fr = exact(bits, isd)
        v = rint_even(fr) if rounding else trunc(fr)
    return sat_i(v, nbits) if signed else sat_u(v, nbits)


def ref_div(a, b):   # b / a on doubles, IEEE
    try:
        return dbits(b / a)
    except ZeroDivisionError:
        if b == 0 or math.isnan(b):
            return 0x7ff8000000000000
        neg = (math.copysign(1, a) < 0) != (math.copysign(1, b) < 0)
        return 0xfff0000000000000 if neg else 0x7ff0000000000000


def ref_rem(x, y):
    try:
        r = math.remainder(x, y)
    except ValueError:
        return 0x7ff8000000000000
    if r == 0:
        r = math.copysign(0.0, x)
    return dbits(r)


def ref_rnd(x):
    if math.isnan(x) or math.isinf(x) or abs(x) >= 2.0 ** 52:
        return dbits(x)
    return dbits(math.copysign(float(round(x)), x))


def ref_sqrt(x):
    if math.isnan(x):
        return dbits(x)
    if x == 0:
        return dbits(x)
    if x < 0:
        return 0x7ff8000000000000
    if math.isinf(x):
        return dbits(x)
    return dbits(math.sqrt(x))


def ref_scalbn(bits, n, isd):
    n = n - (1 << 32) if n & 0x80000000 else n
    nanp = isnand(bits) if isd else isnanf(bits)
    infp = ((bits & 0x7fffffffffffffff) == 0x7ff0000000000000) if isd else ((bits & 0x7fffffff) == 0x7f800000)
    if nanp or infp or (bits << 1) & (M64 if isd else 0xffffffff) == 0:
        return bits
    n = max(-5000, min(5000, n))
    fr = exact(bits, isd) * (Fraction(2) ** n)
    r = to_d(fr) if isd else to_f(fr)
    return r


def flags_for(a, b, ge):
    if math.isnan(a) or math.isnan(b):
        return 0x8 if ge else 0x3          # N  |  C V
    if a < b: return 0x8
    if a == b: return 0x6
    return 0x2


EDGE_D = [0, 1 << 63, 0x3ff0000000000000, 0xbff0000000000000, 0x3fe0000000000000, 0x3ff8000000000000, 0x4004000000000000, 0x7ff0000000000000,
          0xfff0000000000000, 0x7ff8000000000000, 0x7ff0000000000001, 1, 0x000fffffffffffff, 0x0010000000000000, 0x7fefffffffffffff,
          0x41dfffffffc00000, 0x41e0000000000000, 0xc1e0000000000000, 0x43e0000000000000, 0xc3e0000000000000, 0x43f0000000000000,
          0x4330000000000001, 0x3fefffffffffffff, 0xbfe0000000000000, 0x3fe0000000000001, 0x4008000000000000, 0xc004000000000000, 0x4000000000000000]
EDGE_F = [0, 0x80000000, 0x3f800000, 0xbf800000, 0x3f000000, 0x3fc00000, 0x7f800000, 0xff800000, 0x7fc00000, 1, 0x007fffff, 0x00800000, 0x7f7fffff,
          0x4f000000, 0xcf000000, 0x4f800000, 0x5f000000, 0xdf000000, 0x5f800000, 0x4effffff, 0x4b000001, 0x3f7fffff, 0x40200000, 0xc0200000]


def rnd_d(r):
    if r.random() < 0.3:
        return r.getrandbits(64)
    e = r.choice([0, 1, 2, 0x3ff, 0x3fe, 0x400, 0x41d, 0x41e, 0x41f, 0x43d, 0x43e, 0x43f, 0x7fe, r.randint(0x380, 0x440), r.randint(0, 0x7ff)])
    m = r.getrandbits(52)
    if r.random() < 0.3:
        m &= ~((1 << r.randint(0, 52)) - 1)
    return (r.getrandbits(1) << 63) | (e << 52) | m


def rnd_f(r):
    if r.random() < 0.3:
        return r.getrandbits(32)
    e = r.choice([0, 1, 0x7f, 0x7e, 0x80, 0x9d, 0x9e, 0x9f, 0xbd, 0xbe, 0xbf, 0xfe, r.randint(0, 255)])
    return (r.getrandbits(1) << 31) | (e << 23) | r.getrandbits(23)


def main():
    n = int(sys.argv[1]) if len(sys.argv) > 1 else 1500
    img, syms = build()
    missing = [e for e in EXPORTS if e not in syms]
    if missing:
        print("FAIL: missing symbols", missing); return 1
    cpu = Cpu(img)
    rng = random.Random(7)
    fails = 0

    def check(name, args, ok, got, want):
        nonlocal fails
        if not ok:
            fails += 1
            if fails <= 40:
                print("MISMATCH", name, [hex(v) for v in args], "got", hex(got), "want", hex(want))

    def run(name, gen, ref, kind):
        for i in range(n):
            args, wants = gen(i)
            out, flags, pres, sp = cpu.call(syms[name], args)
            want = ref(args)
            if kind == "d":
                got = out[0] | out[1] << 32
                check(name, args, got == want or (isnand(got) and isnand(want)), got, want)
            elif kind == "f":
                got = out[0]
                check(name, args, got == want or (isnanf(got) and isnanf(want)), got, want)
            elif kind == "i":
                check(name, args, out[0] == want, out[0], want)
            elif kind == "ll":
                got = out[0] | out[1] << 32
                check(name, args, got == want, got, want)
            else:   # flags
                check(name, args, flags == want and out == [v & 0xffffffff for v in args] and pres == [0x11110000 + k for k in range(4, 13)] and sp == 0x70080000, flags, want)
        return

    def d1(i): b = EDGE_D[i % len(EDGE_D)] if i < len(EDGE_D) else rnd_d(rng); return regs_d(b) + [0, 0], 0
    def f1(i): b = EDGE_F[i % len(EDGE_F)] if i < len(EDGE_F) else rnd_f(rng); return [b, 0, 0, 0], 0
    def d2(i):
        a = EDGE_D[i % len(EDGE_D)] if i < len(EDGE_D) * 2 else rnd_d(rng)
        b = EDGE_D[(i * 7 + i // len(EDGE_D)) % len(EDGE_D)] if i < len(EDGE_D) * 2 else rnd_d(rng)
        return regs_d(a) + regs_d(b), 0
    def f2(i):
        a = EDGE_F[i % len(EDGE_F)] if i < len(EDGE_F) * 2 else rnd_f(rng)
        b = EDGE_F[(i * 7 + i // len(EDGE_F)) % len(EDGE_F)] if i < len(EDGE_F) * 2 else rnd_f(rng)
        return [a, b, 0, 0], 0
    def i1(i): return [rng.choice([0, 1, 0xffffffff, 0x7fffffff, 0x80000000, rng.getrandbits(32), rng.getrandbits(16)]), 0, 0, 0], 0
    def ll1(i):
        v = rng.choice([0, 1, 2 ** 63 - 1, 2 ** 63, 2 ** 64 - 1, 2 ** 32, 2 ** 53 + 1, rng.getrandbits(64), rng.getrandbits(40), rng.getrandbits(54) | 1, 2 ** 24 + 1])
        return [v & 0xffffffff, v >> 32, 0, 0], 0
    def sc(i):
        k = rng.choice([0, 1, -1, 1023, 1024, -1022, -1023, -1074, -1075, 2047, -2100, 3000, rng.randint(-2200, 2200), rng.getrandbits(32)])
        return regs_d(rnd_d(rng) if i > 30 else EDGE_D[i % len(EDGE_D)]) + [k & 0xffffffff, 0], 0
    def scf(i):
        k = rng.choice([0, 1, -1, 127, 128, -126, -149, -150, 255, -300, 500, rng.randint(-400, 400), rng.getrandbits(32)])
        return [rnd_f(rng) if i > 30 else EDGE_F[i % len(EDGE_F)], k & 0xffffffff, 0, 0], 0

    A = lambda a: dbl(a[0] | a[1] << 32)
    B = lambda a: dbl(a[2] | a[3] << 32)
    run("_dabs", d1, lambda a: (a[0] | a[1] << 32) & ~(1 << 63), "d")
    run("_fabs", f1, lambda a: a[0] & 0x7fffffff, "f")
    run("_dneq", d2, lambda a: int(not (A(a) == B(a))), "i")
    run("_fneq", f2, lambda a: int(not (flt(a[0]) == flt(a[1]))), "i")
    run("_drdiv", d2, lambda a: ref_div(A(a), B(a)), "d")
    run("_frdiv", f2, lambda a: to_f(Fraction(flt(a[1])) / Fraction(flt(a[0]))) if (flt(a[0]) != 0 and flt(a[1]) != 0 and not any(map(lambda v: math.isnan(v) or math.isinf(v), (flt(a[0]), flt(a[1]))))) else struct.unpack("<I", struct.pack("<f", dbl(ref_div(flt(a[0]), flt(a[1])))))[0], "f")
    run("_drem", d2, lambda a: ref_rem(A(a), B(a)), "d")
    run("_frem", f2, lambda a: struct.unpack("<I", struct.pack("<f", dbl(ref_rem(flt(a[0]), flt(a[1])))))[0], "f")
    run("_drnd", d1, lambda a: ref_rnd(A(a)), "d")
    run("_frnd", f1, lambda a: struct.unpack("<I", struct.pack("<f", dbl(ref_rnd(flt(a[0])))))[0], "f")
    run("_dsqrt", d1, lambda a: ref_sqrt(A(a)), "d")
    run("_fsqrt", f1, lambda a: struct.unpack("<I", struct.pack("<f", dbl(ref_sqrt(flt(a[0])))))[0], "f")
    run("_dflt", i1, lambda a: to_d(Fraction(a[0] - (1 << 32) if a[0] >> 31 else a[0])), "d")
    run("_dfltu", i1, lambda a: to_d(Fraction(a[0])), "d")
    run("_fflt", i1, lambda a: to_f(Fraction(a[0] - (1 << 32) if a[0] >> 31 else a[0])), "f")
    run("_ffltu", i1, lambda a: to_f(Fraction(a[0])), "f")
    sll = lambda a: (a[0] | a[1] << 32) - (1 << 64) if a[1] >> 31 else a[0] | a[1] << 32
    ull = lambda a: a[0] | a[1] << 32
    run("_ll_sto_d", ll1, lambda a: to_d(Fraction(sll(a))), "d")
    run("_ll_uto_d", ll1, lambda a: to_d(Fraction(ull(a))), "d")
    run("_ll_sto_f", ll1, lambda a: to_f(Fraction(sll(a))), "f")
    run("_ll_uto_f", ll1, lambda a: to_f(Fraction(ull(a))), "f")
    db_ = lambda a: a[0] | a[1] << 32
    for nm, rd, sg, nb, kind, g, isd in (
            ("_dfix_r", 1, 1, 32, "i", d1, 1), ("_dfixu_r", 1, 0, 32, "i", d1, 1), ("_ffix_r", 1, 1, 32, "i", f1, 0), ("_ffixu_r", 1, 0, 32, "i", f1, 0),
            ("_ll_sfrom_d", 0, 1, 64, "ll", d1, 1), ("_ll_ufrom_d", 0, 0, 64, "ll", d1, 1), ("_ll_sfrom_f", 0, 1, 64, "ll", f1, 0), ("_ll_ufrom_f", 0, 0, 64, "ll", f1, 0),
            ("_ll_sfrom_d_r", 1, 1, 64, "ll", d1, 1), ("_ll_ufrom_d_r", 1, 0, 64, "ll", d1, 1), ("_ll_sfrom_f_r", 1, 1, 64, "ll", f1, 0), ("_ll_ufrom_f_r", 1, 0, 64, "ll", f1, 0)):
        run(nm, g, (lambda rd, sg, nb, isd: lambda a: to_int(db_(a) if isd else a[0], isd, rd, sg, nb))(rd, sg, nb, isd), kind)
    run("__ARM_scalbn", sc, lambda a: ref_scalbn(db_(a), a[2], True), "d")
    run("__ARM_scalbnf", scf, lambda a: ref_scalbn(a[0], a[1], False), "f")
    for nm in ("__dcmp4", "_dcmp4", "_dcmpge"):
        run(nm, d2, (lambda ge: lambda a: flags_for(A(a), B(a), ge))(nm == "_dcmpge"), "flags")
    for nm in ("__fcmp4", "_fcmp4", "_fcmpge"):
        run(nm, f2, (lambda ge: lambda a: flags_for(flt(a[0]), flt(a[1]), ge))(nm == "_fcmpge"), "flags")
    print("dfprvct2_2: %d exports, %d cases each, %d mismatches" % (len(EXPORTS), n, fails))
    return 1 if fails else 0


sys.exit(main())
