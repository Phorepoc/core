"""components_phetty -- OURS: the developer tty daemon (docs/phorepoc/phetty-plan.md). Merged into components.COMPONENTS
unconditionally (a key only ships when an image has been built for it; PHETTY=exclude drops the four keys from the ROM,
x32_rom_args.py)."""
import os

from paths import OS_ROOT, SYMBIAN, BUILD, EPOC32_ROOT  # noqa: E402

COMPONENTS = {
    # the daemon: owns the ttys; started by the SSM non-critical list (r_cmd_phettyd) or by the first client
    "phettyd": dict(mmp=os.path.join(OS_ROOT, "src/phetty/phettyd.mmp")),
    # eshell (the real etshell sources) with its console redirected to a phettyd tty (macro Console=PheConsole)
    "pheshell": dict(mmp=os.path.join(OS_ROOT, "src/pheshell/pheshell.mmp")),
    # the on-screen terminal (an Avkon app in the launcher grid = the Menu key's menu; Options lists the ttys)
    "pheterm": dict(mmp=os.path.join(OS_ROOT, "src/pheterm/pheterm.mmp")),
    # headless self-test client (SSM_TEST=phettyctl; not in the default ROM)
    "phettyctl": dict(mmp=os.path.join(OS_ROOT, "src/phetty/phettyctl.mmp"), test=True),
}
PHETTY_KEYS = ("phettyd", "pheshell", "pheterm", "phettyctl")
