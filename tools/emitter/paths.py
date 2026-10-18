"""paths -- where everything lives, for every tool in tools/emitter (and tools/*.py).

    CORE      this repository
    OS_ROOT   the Phorepoc OS tree: toolchain/ (shims, overlay, kinc, generated, dll_rt ...), src/ (our components),
              the board and port directories.  PHOREPOC_OS_ROOT, default <core>/os.
              (Temporary: until the overlay and the components move into core/os, point it at the checkout that
              still holds them.)
    SYMBIAN   the Symbian source: symbian-2010/<package>/ and symbian-rcl3/<package>/ (fetch.sh puts them in
              <core>/symbian).  PHOREPOC_SYMBIAN
    BUILD     all generated files.  PHOREPOC_BUILD, default <core>/build
    EPOC32_ROOT   BUILD/epoc32: the exported include tree (tools/epoc32_export.py)

Registry paths are relative; resolve() turns one into an absolute path:
    baseline/<x>   -> SYMBIAN/<x>      (the registry still says baseline/symbian-2010/...; also accepts symbian-2010/...)
    thirdparty/<x>, .<x>/ -> BUILD/<x> (prebuilt third-party archives, generated icons ...)
    anything else  -> OS_ROOT/<x>
"""
import os

HERE = os.path.dirname(os.path.abspath(__file__))
CORE = os.path.dirname(os.path.dirname(HERE))


def _dir(env: str, default: str) -> str:
    return os.path.abspath(os.environ.get(env) or default)


OS_ROOT = _dir("PHOREPOC_OS_ROOT", os.path.join(CORE, "os"))
SYMBIAN = _dir("PHOREPOC_SYMBIAN", os.path.join(CORE, "symbian"))
BUILD = _dir("PHOREPOC_BUILD", os.path.join(CORE, "build"))
EPOC32_ROOT = os.path.join(BUILD, "epoc32")


def resolve(rel: str) -> str:
    if os.path.isabs(rel):
        return rel
    rel = rel.replace("\\", "/")
    if rel.startswith("baseline/"):
        return os.path.join(SYMBIAN, rel[len("baseline/"):])
    if rel.startswith(("thirdparty/", ".")):
        return os.path.join(BUILD, rel.lstrip("."))
    return os.path.join(OS_ROOT, rel)
