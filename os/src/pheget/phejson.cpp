// Copyright The Phorepoc Authors
// SPDX-License-Identifier: EPL-1.0

// phejson.cpp -- OURS: see phejson.h
#include "phejson.h"

static const TInt KMaxDepth = 32;

class CPheJson::TReader
	{
public:
	TReader(const TDesC8& aText) : iText(aText), iPos(0) {}
	void SkipWs()
		{
		while (iPos < iText.Length())
			{
			TUint8 c = iText[iPos];
			if (c != ' ' && c != '\t' && c != '\r' && c != '\n')
				break;
			++iPos;
			}
		}
	TInt Peek() { SkipWs(); return iPos < iText.Length() ? iText[iPos] : -1; }
	TUint8 NextL()
		{
		if (iPos >= iText.Length())
			User::Leave(KErrCorrupt);
		return iText[iPos++];
		}
	void ExpectL(TUint8 aChar) { if (Peek() != aChar) User::Leave(KErrCorrupt); ++iPos; }
	void ExpectWordL(const TDesC8& aWord)
		{
		if (iText.Mid(iPos).Left(aWord.Length()) != aWord)
			User::Leave(KErrCorrupt);
		iPos += aWord.Length();
		}
	HBufC8* ReadStringL();
	HBufC8* ReadNumberL();

	const TDesC8& iText;
	TInt iPos;
	};

static void AppendUtf8(TDes8& aOut, TUint aCode)
	{
	if (aCode < 0x80)
		aOut.Append(TChar(aCode));
	else if (aCode < 0x800)
		{
		aOut.Append(TChar(0xC0 | (aCode >> 6)));
		aOut.Append(TChar(0x80 | (aCode & 0x3F)));
		}
	else if (aCode < 0x10000)
		{
		aOut.Append(TChar(0xE0 | (aCode >> 12)));
		aOut.Append(TChar(0x80 | ((aCode >> 6) & 0x3F)));
		aOut.Append(TChar(0x80 | (aCode & 0x3F)));
		}
	else
		{
		aOut.Append(TChar(0xF0 | (aCode >> 18)));
		aOut.Append(TChar(0x80 | ((aCode >> 12) & 0x3F)));
		aOut.Append(TChar(0x80 | ((aCode >> 6) & 0x3F)));
		aOut.Append(TChar(0x80 | (aCode & 0x3F)));
		}
	}

static TUint Hex4L(CPheJson::TReader& aR);

HBufC8* CPheJson::TReader::ReadStringL()
	{
	ExpectL('"');
	// the decoded text is never longer than the raw one
	TInt end = iPos;
	while (end < iText.Length() && iText[end] != '"')
		end += (iText[end] == '\\') ? 2 : 1;
	if (end >= iText.Length())
		User::Leave(KErrCorrupt);
	HBufC8* buf = HBufC8::NewLC(end - iPos);
	TPtr8 out = buf->Des();
	for (;;)
		{
		TUint8 c = NextL();
		if (c == '"')
			break;
		if (c < 0x20)
			User::Leave(KErrCorrupt);
		if (c != '\\')
			{
			out.Append(c);
			continue;
			}
		TUint8 e = NextL();
		switch (e)
			{
			case '"': case '\\': case '/': out.Append(e); break;
			case 'b': out.Append('\b'); break;
			case 'f': out.Append('\f'); break;
			case 'n': out.Append('\n'); break;
			case 'r': out.Append('\r'); break;
			case 't': out.Append('\t'); break;
			case 'u':
				{
				TUint code = Hex4L(*this);
				if (code >= 0xD800 && code < 0xDC00 && iPos + 6 <= iText.Length() && iText[iPos] == '\\' && iText[iPos + 1] == 'u')
					{
					iPos += 2;
					TUint low = Hex4L(*this);
					code = 0x10000 + ((code - 0xD800) << 10) + (low - 0xDC00);
					}
				if (out.Length() + 4 > out.MaxLength())   // "\uXXXX" (6) always covers the 1..4 UTF-8 bytes
					User::Leave(KErrCorrupt);
				AppendUtf8(out, code);
				break;
				}
			default:
				User::Leave(KErrCorrupt);
			}
		}
	CleanupStack::Pop(buf);
	return buf;
	}

static TUint Hex4L(CPheJson::TReader& aR)
	{
	TUint v = 0;
	for (TInt i = 0; i < 4; ++i)
		{
		TUint8 c = aR.NextL();
		v <<= 4;
		if (c >= '0' && c <= '9') v |= c - '0';
		else if (c >= 'a' && c <= 'f') v |= c - 'a' + 10;
		else if (c >= 'A' && c <= 'F') v |= c - 'A' + 10;
		else User::Leave(KErrCorrupt);
		}
	return v;
	}

HBufC8* CPheJson::TReader::ReadNumberL()
	{
	SkipWs();
	TInt start = iPos;
	while (iPos < iText.Length())
		{
		TUint8 c = iText[iPos];
		if (!((c >= '0' && c <= '9') || c == '-' || c == '+' || c == '.' || c == 'e' || c == 'E'))
			break;
		++iPos;
		}
	if (iPos == start)
		User::Leave(KErrCorrupt);
	return iText.Mid(start, iPos - start).AllocL();
	}

CPheJson* CPheJson::ParseL(const TDesC8& aText)
	{
	TReader r(aText);
	CPheJson* v = ReadValueL(r, 0);
	if (r.Peek() != -1)
		{
		delete v;
		User::Leave(KErrCorrupt);
		}
	return v;
	}

CPheJson* CPheJson::ReadValueL(TReader& aR, TInt aDepth)
	{
	if (aDepth > KMaxDepth)
		User::Leave(KErrCorrupt);
	TInt c = aR.Peek();
	CPheJson* v = NULL;
	switch (c)
		{
		case '{':
			{
			v = new (ELeave) CPheJson(EObject);
			CleanupStack::PushL(v);
			aR.ExpectL('{');
			if (aR.Peek() == '}')
				{
				aR.ExpectL('}');
				break;
				}
			for (;;)
				{
				HBufC8* key = aR.ReadStringL();
				CleanupStack::PushL(key);
				aR.ExpectL(':');
				CPheJson* item = ReadValueL(aR, aDepth + 1);
				CleanupStack::PushL(item);
				v->iItems.AppendL(item);
				CleanupStack::Pop(item);
				v->iKeys.AppendL(key);
				CleanupStack::Pop(key);
				if (aR.Peek() == ',')
					{
					aR.ExpectL(',');
					continue;
					}
				aR.ExpectL('}');
				break;
				}
			break;
			}
		case '[':
			{
			v = new (ELeave) CPheJson(EArray);
			CleanupStack::PushL(v);
			aR.ExpectL('[');
			if (aR.Peek() == ']')
				{
				aR.ExpectL(']');
				break;
				}
			for (;;)
				{
				CPheJson* item = ReadValueL(aR, aDepth + 1);
				CleanupStack::PushL(item);
				v->iItems.AppendL(item);
				CleanupStack::Pop(item);
				if (aR.Peek() == ',')
					{
					aR.ExpectL(',');
					continue;
					}
				aR.ExpectL(']');
				break;
				}
			break;
			}
		case '"':
			v = new (ELeave) CPheJson(EString);
			CleanupStack::PushL(v);
			v->iStr = aR.ReadStringL();
			break;
		case 't':
			v = new (ELeave) CPheJson(EBool);
			CleanupStack::PushL(v);
			aR.ExpectWordL(_L8("true"));
			v->iBool = ETrue;
			break;
		case 'f':
			v = new (ELeave) CPheJson(EBool);
			CleanupStack::PushL(v);
			aR.ExpectWordL(_L8("false"));
			break;
		case 'n':
			v = new (ELeave) CPheJson(ENull);
			CleanupStack::PushL(v);
			aR.ExpectWordL(_L8("null"));
			break;
		default:
			{
			v = new (ELeave) CPheJson(ENumber);
			CleanupStack::PushL(v);
			v->iStr = aR.ReadNumberL();
			TLex8 lex(*v->iStr);
			if (lex.Val(v->iInt) != KErrNone)
				v->iInt = 0;
			break;
			}
		}
	CleanupStack::Pop(v);
	return v;
	}

CPheJson::~CPheJson()
	{
	delete iStr;
	iItems.ResetAndDestroy();
	iKeys.ResetAndDestroy();
	}

const TDesC8& CPheJson::Str() const
	{
	return iStr ? static_cast<const TDesC8&>(*iStr) : static_cast<const TDesC8&>(KNullDesC8);
	}

const CPheJson* CPheJson::Get(const TDesC8& aKey) const
	{
	for (TInt i = 0; i < iKeys.Count(); ++i)
		if (*iKeys[i] == aKey)
			return iItems[i];
	return NULL;
	}

TPtrC8 CPheJson::GetStr(const TDesC8& aKey) const
	{
	const CPheJson* v = Get(aKey);
	return (v && v->iType == EString) ? TPtrC8(v->Str()) : TPtrC8(KNullDesC8);
	}

TBool CPheJson::ArrayHas(const TDesC8& aValue) const
	{
	for (TInt i = 0; i < iItems.Count(); ++i)
		if (iItems[i]->iType == EString && iItems[i]->Str() == aValue)
			return ETrue;
	return EFalse;
	}
