"""missing_exports.py <key> -- which frozen-.def exports of a qt48_* component its objects do not define (ARM build,
emitter/build_mmp.py output in .mmp-build/<key>). Run from the repo root: py tools/phorepoc-qt48/missing_exports.py qt48_core"""
import glob, os, subprocess, sys
sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "..", "emitter"))
import components as C, build_euser_dll as BE
from euser_def import parse_def
key = sys.argv[1]
dm = parse_def(C.COMPONENTS[key]["def"])
objs = sorted(glob.glob(os.path.join(".mmp-build", key, "*.o")))
defined = BE._nm("--defined-only", *objs)
miss = [(o, n) for o, n in sorted(dm.by_ordinal.items()) if n not in defined]
print(f"{key}: {len(dm.by_ordinal) - len(miss)}/{len(dm.by_ordinal)} defined, {len(miss)} missing")
cf = subprocess.run([os.path.join(os.path.dirname(BE.GCC), "arm-none-eabi-c++filt.exe")], input="\n".join(n for _, n in miss), capture_output=True, text=True).stdout.splitlines()
for (o, n), d in zip(miss, cf):
    print(f"  {o:5d} {d if d != n else n}")
