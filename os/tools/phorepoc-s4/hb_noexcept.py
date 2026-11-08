# Copyright The Phorepoc Authors
# SPDX-License-Identifier: Apache-2.0
"""hb_noexcept.py -- Symbian^4 S4.4 (docs/phorepoc/symbian4-build.md): Orbit without C++ exceptions, as Qt (patch 0001).

Our x32 board has no C++ unwinder (TRAP is setjmp-based), so Qt is built QT_NO_EXCEPTIONS. Orbit's own try/catch are
all out-of-memory guards (std::bad_alloc from its shared-memory allocators). This rewrites the EXPORTED copy
(.phorepoc-s4-build/hb-src; the baseline tree is never written), idempotently:
  try {                         ->  {
  catch (T &name) {             ->  if (false) { T &name = *static_cast<T *>(0);   (never runs; keeps `name` legal)
  catch (T &) { / catch(...) {  ->  if (false) {
  throw std::bad_alloc();       ->  qBadAlloc();        (QtCore: with QT_NO_EXCEPTIONS it returns)
  throw;                        ->  qBadAlloc();
  QT_CATCH(std::X &name) {      ->  QT_CATCH(...) { int name = 0;   (Qt's macro form)
  qt_symbian_exception2LeaveL(e)->  User::Leave(KErrNoMemory)
Anything else that throws is reported (none in 2010 hb besides the above and HbNvgException, handled the same way).

    python3 hb_noexcept.py <hb-src>/src
"""
import os
import re
import sys

TRY = re.compile(r"\btry\s*\{")
CATCH_NAMED = re.compile(r"\bcatch\s*\(\s*((?:const\s+)?[\w:]+)\s*&\s*(\w+)\s*\)\s*\{")
CATCH_ANON = re.compile(r"\bcatch\s*\(\s*(?:(?:const\s+)?[\w:]+\s*&?\s*|\.\.\.)\)\s*\{")
# (one line only: a comment's "in case of throw" must not run into the next statement)
THROW = re.compile(r"\bthrow[ \t]+[\w:]+[ \t]*\([^;\n]*\)[ \t]*;|\bthrow[ \t]*;")


def _named(m) -> str:
    # std:: exception types are not declared without exceptions (<new> has no bad_alloc): the name becomes an int
    if m.group(1).replace("const", "").strip().startswith("std::"):
        return f"if (false) {{ int {m.group(2)} = 0; (void){m.group(2)};"
    return f"if (false) {{ {m.group(1)} &{m.group(2)} = *static_cast<{m.group(1)} *>(0);"


# an earlier version of this script's output for std:: types, brought to the form above
OLD_STD = re.compile(r"((?:const\s+)?std::\w+) &(\w+) = \*static_cast<\1 \*>\(0\);")


# Qt's own macros (QT_CATCH(A) = `else` with QT_NO_EXCEPTIONS) with a NAMED std:: exception the handler uses
QT_CATCH_STD = re.compile(r"\bQT_CATCH\s*\(\s*(?:const\s+)?std::\w+\s*&\s*(\w+)\s*\)\s*\{")
EXC2LEAVE = re.compile(r"\bqt_symbian_exception2LeaveL\s*\(\s*\w+\s*\)")


def rewrite(text: str) -> str:
    text = QT_CATCH_STD.sub(lambda m: f"QT_CATCH(...) {{ int {m.group(1)} = 0; (void){m.group(1)};", text)
    text = EXC2LEAVE.sub("User::Leave(KErrNoMemory)", text)     # the only exception it ever gets here: bad_alloc
    text = TRY.sub("{", text)
    text = CATCH_NAMED.sub(_named, text)
    text = OLD_STD.sub(lambda m: f"int {m.group(2)} = 0; (void){m.group(2)};", text)
    text = CATCH_ANON.sub("if (false) {", text)
    return THROW.sub("qBadAlloc();", text)


def main(root: str) -> None:
    changed = 0
    for d, _, files in os.walk(root):
        for f in files:
            if not f.endswith((".cpp", ".h")) or f.startswith(("moc_", "qrc_")):
                continue
            p = os.path.join(d, f)
            with open(p, encoding="utf-8", errors="surrogateescape", newline="") as fh:
                old = fh.read()
            if not any(k in old for k in ("try", "catch", "throw", "static_cast<std::", "static_cast<const std::", "QT_CATCH", "exception2Leave")):
                continue
            new = rewrite(old)
            if new != old:
                with open(p, "w", encoding="utf-8", errors="surrogateescape", newline="") as fh:
                    fh.write(new)
                changed += 1
    print(f"hb_noexcept: {changed} files rewritten")


if __name__ == "__main__":
    main(sys.argv[1])
