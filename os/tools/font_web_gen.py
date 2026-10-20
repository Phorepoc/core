r"""font_web_gen -- the web-script fallback faces the BrowserNG engine (WebKit/Skia) uses for text the S60 faces lack (2026-10-16).

    py tools/font_web_gen.py [outdir]          (default .font-gen/web/)

Source fonts (all SIL Open Font License 1.1, third_party/fonts/noto/; nothing proprietary):
  * Noto Sans Arabic / Hebrew / Thai / Devanagari -- whole files (25-240 KB each)
  * Noto Sans CJK SC -- SUBSET with fontTools to the characters of GB2312 (simplified), JIS X 0208 (Japanese: kanji+kana),
    Big5 level 1 (traditional, 4808 common hanzi), KS X 1001 hangul + symbols and the CJK/fullwidth punctuation blocks,
    no hinting, no layout features: ~3 MB instead of 16 MB.
Written under their own family names (the OFL reserved font name "Noto" is kept: unmodified-name subsets are allowed).
The engine finds them by coverage (SkFontMgr_Custom::onMatchFamilyStyleCharacter, patched in tools/phorepoc-x86/cxxrt/
webkit/patch.py); staged in resource/phweb/fonts (NOT under Resource/Fonts: the S60 font store stalled the boot, phone boot 675).
"""
import os
import shutil
import sys

from fontTools import subset
from fontTools.ttLib import TTFont

REPO = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
SRC = os.path.join(REPO, "third_party", "fonts", "noto")
CJK_SRC = os.path.join(REPO, ".font-dl", "NotoSansCJKsc-Regular.otf")   # 16 MB, not committed: curl -L https://github.com/notofonts/noto-cjk/raw/main/Sans/OTF/SimplifiedChinese/NotoSansCJKsc-Regular.otf
WHOLE = ["NotoSansArabic-Regular.ttf", "NotoSansHebrew-Regular.ttf", "NotoSansThai-Regular.ttf",
         "NotoSansDevanagari-Regular.ttf"]
CJK = "NotoSansCJKsc-Regular.otf"


def codepoints():
    cps = set()

    def rng(codec, lead, trail, extra=()):
        for hi in range(lead[0], lead[1] + 1):
            for lo in range(trail[0], trail[1] + 1):
                try:
                    s = bytes([hi, lo]).decode(codec)
                except UnicodeDecodeError:
                    continue
                if len(s) == 1:
                    cps.add(ord(s))
    rng("gb2312", (0xA1, 0xF7), (0xA1, 0xFE))                 # GB2312: 6763 hanzi + symbols/kana/Cyrillic...
    rng("shift_jis", (0x81, 0x9F), (0x40, 0xFC))              # JIS X 0208 (kanji level 1+2, kana, symbols)
    rng("shift_jis", (0xE0, 0xEA), (0x40, 0xFC))
    rng("big5", (0xA1, 0xC6), (0x40, 0xFE))                   # Big5: symbols + level 1 hanzi (to 0xC67E)
    rng("euc_kr", (0xB0, 0xC8), (0xA1, 0xFE))                 # KS X 1001 hangul (2350)
    rng("euc_kr", (0xA1, 0xAD), (0xA1, 0xFE))                 # its symbols/jamo
    for a, b in ((0x2000, 0x206F), (0x2190, 0x21FF), (0x2460, 0x24FF), (0x25A0, 0x25FF), (0x3000, 0x303F),
                 (0x3040, 0x30FF), (0x3100, 0x312F), (0x3130, 0x318F), (0xFE30, 0xFE4F), (0xFF00, 0xFFEF)):
        cps.update(range(a, b + 1))
    return sorted(cps)


def main():
    out = sys.argv[1] if len(sys.argv) > 1 else os.path.join(REPO, ".font-gen", "web")
    os.makedirs(out, exist_ok=True)
    for f in WHOLE:
        shutil.copyfile(os.path.join(SRC, f), os.path.join(out, f))
        print(f"  {f}: {os.path.getsize(os.path.join(out, f))} B")
    opt = subset.Options()
    opt.hinting = False
    opt.layout_features = []
    opt.notdef_outline = True
    opt.name_IDs = ["*"]
    opt.glyph_names = False
    opt.desubroutinize = False
    if not os.path.isfile(CJK_SRC):
        shutil.copyfile(os.path.join(SRC, "NotoSansCJKsc-Subset.otf"), os.path.join(out, "NotoSansCJKsc-Subset.otf"))
        print("  CJK source not downloaded: committed subset copied")
        return
    f = TTFont(CJK_SRC)
    s = subset.Subsetter(opt)
    s.populate(unicodes=codepoints())
    s.subset(f)
    dst = os.path.join(out, "NotoSansCJKsc-Subset.otf")
    f.save(dst)
    print(f"  {CJK} -> {os.path.basename(dst)}: {os.path.getsize(dst)} B, {len(f.getGlyphOrder())} glyphs")


if __name__ == "__main__":
    main()
