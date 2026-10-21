// Copyright The Phorepoc Authors
// SPDX-License-Identifier: EPL-2.0 OR GPL-2.0-or-later

// phejson.h -- OURS: a small JSON reader (RFC 8259) for pheget's indexes. Builds a tree of CPheJson nodes; strings are
// kept as UTF-8 with escapes decoded. Numbers are kept as their text (and as TInt64 when integral).
#ifndef PHEJSON_H
#define PHEJSON_H

#include <e32base.h>

class CPheJson : public CBase
	{
public:
	enum TType { ENull, EBool, ENumber, EString, EArray, EObject };

	static CPheJson* ParseL(const TDesC8& aText);   // leaves KErrCorrupt on malformed input
	~CPheJson();

	TType Type() const { return iType; }
	TBool Bool() const { return iBool; }
	TInt64 Int() const { return iInt; }
	const TDesC8& Str() const;                      // EString / ENumber text; empty otherwise
	TInt Count() const { return iItems.Count(); }   // EArray / EObject
	const CPheJson& At(TInt aIndex) const { return *iItems[aIndex]; }
	const TDesC8& KeyAt(TInt aIndex) const { return *iKeys[aIndex]; }
	const CPheJson* Get(const TDesC8& aKey) const;  // EObject member or NULL
	TPtrC8 GetStr(const TDesC8& aKey) const;        // a string member's value, or empty
	TBool ArrayHas(const TDesC8& aValue) const;     // EArray holding that string

	class TReader;                                  // (the parser's cursor; phejson.cpp)
private:
	CPheJson(TType aType) : iType(aType) {}
	static CPheJson* ReadValueL(TReader& aR, TInt aDepth);

	TType iType;
	TBool iBool;
	TInt64 iInt;
	HBufC8* iStr;
	RPointerArray<CPheJson> iItems;
	RPointerArray<HBufC8> iKeys;
	};

#endif
