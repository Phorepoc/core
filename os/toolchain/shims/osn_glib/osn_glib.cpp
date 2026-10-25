/* Copyright The Phorepoc Authors
 * SPDX-License-Identifier: Apache-2.0 */

// osn_glib.cpp -- OURS (not Symbian source): the eleven GLib calls osncore makes (UString's UTF-8 helpers), so osncore
// does not import libglib (LGPL; nothing LGPL goes in the image). Compiled into osncore (registry `extra`); the
// definitions satisfy the link, so libglib.dll is never imported.
// Approximations, good enough for UString: g_utf8_normalize copies (no NFC/NFD tables), g_utf8_collate and
// g_utf8_collate_key both order by code point (consistent with each other, not locale-aware), g_setenv is a no-op
// (osncore only sets G_SLICE, a GLib allocator switch).
#include <e32std.h>
#include <glib.h>

static gchar* Dup(const gchar* aStr, gssize aLen)
    {
    TInt n = aLen < 0 ? User::StringLength(reinterpret_cast<const TUint8*>(aStr)) : aLen;
    gchar* r = static_cast<gchar*>(User::Alloc(n + 1));
    if (r)
        {
        Mem::Copy(r, aStr, n);
        r[n] = 0;
        }
    return r;
    }

// length of the UTF-8 sequence led by c (1 for a stray continuation byte, so a bad string still advances)
static TInt SeqLen(TUint8 c)
    {
    return c < 0xC0 ? 1 : c < 0xE0 ? 2 : c < 0xF0 ? 3 : 4;
    }

extern "C" {

void g_free(gpointer aMem)
    {
    User::Free(aMem);
    }

gboolean g_setenv(const gchar* /*aVariable*/, const gchar* /*aValue*/, gboolean /*aOverwrite*/)
    {
    return TRUE;
    }

gunichar g_utf8_get_char(const gchar* aP)
    {
    const TUint8* p = reinterpret_cast<const TUint8*>(aP);
    TInt n = SeqLen(p[0]);
    gunichar c = n == 1 ? p[0] : n == 2 ? (p[0] & 0x1F) : n == 3 ? (p[0] & 0x0F) : (p[0] & 0x07);
    for (TInt i = 1; i < n; i++)
        c = (c << 6) | (p[i] & 0x3F);
    return c;
    }

gint g_unichar_to_utf8(gunichar aC, gchar* aOut)
    {
    TUint8 b[4];
    TInt n;
    if (aC < 0x80)
        { b[0] = TUint8(aC); n = 1; }
    else if (aC < 0x800)
        { b[0] = TUint8(0xC0 | (aC >> 6)); b[1] = TUint8(0x80 | (aC & 0x3F)); n = 2; }
    else if (aC < 0x10000)
        { b[0] = TUint8(0xE0 | (aC >> 12)); b[1] = TUint8(0x80 | ((aC >> 6) & 0x3F)); b[2] = TUint8(0x80 | (aC & 0x3F)); n = 3; }
    else
        { b[0] = TUint8(0xF0 | (aC >> 18)); b[1] = TUint8(0x80 | ((aC >> 12) & 0x3F));
          b[2] = TUint8(0x80 | ((aC >> 6) & 0x3F)); b[3] = TUint8(0x80 | (aC & 0x3F)); n = 4; }
    if (aOut)
        Mem::Copy(aOut, b, n);
    return n;
    }

glong g_utf8_strlen(const gchar* aP, gssize aMax)
    {
    glong count = 0;
    const gchar* p = aP;
    while ((aMax < 0 || p - aP < aMax) && *p)
        {
        TInt n = SeqLen(TUint8(*p));
        if (aMax >= 0 && p - aP + n > aMax)
            break;
        p += n;
        count++;
        }
    return count;
    }

gchar* g_utf8_offset_to_pointer(const gchar* aStr, glong aOffset)
    {
    const gchar* p = aStr;
    if (aOffset >= 0)
        {
        while (aOffset-- > 0)
            p += SeqLen(TUint8(*p));
        }
    else
        {
        while (aOffset++ < 0)
            {
            do
                p--;
            while ((TUint8(*p) & 0xC0) == 0x80);
            }
        }
    return const_cast<gchar*>(p);
    }

gunichar2* g_utf8_to_utf16(const gchar* aStr, glong aLen, glong* aRead, glong* aWritten, GError** aError)
    {
    if (aError)
        *aError = NULL;
    const gchar* p = aStr;
    const gchar* end = aLen < 0 ? NULL : aStr + aLen;
    glong units = 0;
    for (const gchar* q = p; (end ? q < end : 1) && *q; q += SeqLen(TUint8(*q)))
        units += g_utf8_get_char(q) >= 0x10000 ? 2 : 1;
    gunichar2* r = static_cast<gunichar2*>(User::Alloc((units + 1) * sizeof(gunichar2)));
    if (!r)
        return NULL;
    glong w = 0;
    for (; (end ? p < end : 1) && *p; p += SeqLen(TUint8(*p)))
        {
        gunichar c = g_utf8_get_char(p);
        if (c >= 0x10000)
            {
            c -= 0x10000;
            r[w++] = gunichar2(0xD800 | (c >> 10));
            r[w++] = gunichar2(0xDC00 | (c & 0x3FF));
            }
        else
            r[w++] = gunichar2(c);
        }
    r[w] = 0;
    if (aRead)
        *aRead = p - aStr;
    if (aWritten)
        *aWritten = w;
    return r;
    }

gchar* g_utf8_normalize(const gchar* aStr, gssize aLen, GNormalizeMode /*aMode*/)
    {
    return Dup(aStr, aLen);
    }

// UTF-8 byte order is code-point order, so strcmp of the strings (and of their copies as keys) agree
gint g_utf8_collate(const gchar* aStr1, const gchar* aStr2)
    {
    const TUint8* a = reinterpret_cast<const TUint8*>(aStr1);
    const TUint8* b = reinterpret_cast<const TUint8*>(aStr2);
    while (*a && *a == *b)
        a++, b++;
    return TInt(*a) - TInt(*b);
    }

gchar* g_utf8_collate_key(const gchar* aStr, gssize aLen)
    {
    return Dup(aStr, aLen);
    }

}   // extern "C"
