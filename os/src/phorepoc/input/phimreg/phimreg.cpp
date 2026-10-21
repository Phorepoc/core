// Copyright The Phorepoc Authors
// SPDX-License-Identifier: EPL-2.0 OR GPL-2.0-or-later

// phimreg.cpp -- OURS (Phorepoc): the input-method registry (see phimreg.h)
#include <ecom/ecom.h>
#include <ecom/implementationinformation.h>
#include <centralrepository.h>
#include "phimreg.h"

namespace
    {
    struct TBuiltin { TUint32 iUid; TInt iMode; TUint32 iLayout; TInt iPrio; const char* iName; };
    // the stock keyboards; the Itut entry's layout is picked by its own IME plug-in, so 0
    const TBuiltin KBuiltins[] =
        {
        { KPhImEntryStockQwerty, KPhImModePortraitQwerty, KPhImStockQwertyLayout, 200, "Nokia QWERTY (stock)" },
        { KPhImEntryStockItut,   KPhImModeItut,           0,                      100, "Nokia keypad ITU-T (stock)" },
        };

    // "PM=128;L=0xE7015803;P=10": the number after aTag (hex when it starts 0x, else decimal); 0 when absent
    TUint32 TagValue( const TDesC8& aData, const TDesC8& aTag )
        {
        TInt pos = aData.Find( aTag );
        if ( pos < 0 ) return 0;
        TLex8 lex( aData.Mid( pos + aTag.Length() ) );
        TUint32 v = 0;
        if ( lex.Peek() == '0' )
            {
            TLex8 probe( lex );
            probe.Inc();
            if ( probe.Peek() == 'x' || probe.Peek() == 'X' )
                {
                probe.Inc();
                probe.Val( v, EHex );
                return v;
                }
            }
        lex.Val( v, EDecimal );
        return v;
        }

    void SortByPriority( RArray<TPhImEntry>& aArr )
        {
        for ( TInt i = 1; i < aArr.Count(); ++i )
            for ( TInt j = i; j > 0 && aArr[j].iPriority < aArr[j-1].iPriority; --j )
                {
                TPhImEntry t = aArr[j];
                aArr[j] = aArr[j-1];
                aArr[j-1] = t;
                }
        }

    void CloseImpls( TAny* aArray )
        {
        RImplInfoPtrArray* a = static_cast<RImplInfoPtrArray*>( aArray );
        a->ResetAndDestroy();
        a->Close();
        }

    void ListL( RArray<TPhImEntry>& aOut )
        {
        for ( TUint i = 0; i < sizeof( KBuiltins ) / sizeof( KBuiltins[0] ); ++i )
            {
            TPhImEntry e;
            e.iEntryUid = KBuiltins[i].iUid;
            e.iMode = KBuiltins[i].iMode;
            e.iLayoutUid = KBuiltins[i].iLayout;
            e.iPriority = KBuiltins[i].iPrio;
            e.iName.Copy( TPtrC8( reinterpret_cast<const TUint8*>( KBuiltins[i].iName ) ) );
            aOut.AppendL( e );
            }
        RImplInfoPtrArray impls;
        CleanupStack::PushL( TCleanupItem( CloseImpls, &impls ) );
        REComSession::ListImplementationsL( TUid::Uid( PHIM_ENTRY_INTERFACE ), impls );
        for ( TInt i = 0; i < impls.Count(); ++i )
            {
            const TDesC8& od = impls[i]->OpaqueData();
            TPhImEntry e;
            e.iEntryUid = impls[i]->ImplementationUid().iUid;
            e.iMode = TagValue( od, _L8( "PM=" ) );
            e.iLayoutUid = TagValue( od, _L8( "L=" ) );
            e.iPriority = TagValue( od, _L8( "P=" ) );
            e.iName.Copy( impls[i]->DisplayName().Left( KPhImNameMax ) );
            if ( e.iMode )
                {
                aOut.AppendL( e );
                }
            }
        CleanupStack::PopAndDestroy( &impls );
        SortByPriority( aOut );
        }

    TInt GetKey( TUint32 aKey, TInt& aVal )
        {
        CRepository* r = NULL;
        TRAPD( err, r = CRepository::NewL( TUid::Uid( KPhImRepository ) ) );
        if ( err ) return err;
        err = r->Get( aKey, aVal );
        delete r;
        return err;
        }

    TInt SetKey( TUint32 aKey, TInt aVal )
        {
        CRepository* r = NULL;
        TRAPD( err, r = CRepository::NewL( TUid::Uid( KPhImRepository ) ) );
        if ( err ) return err;
        err = r->Set( aKey, aVal );
        if ( err == KErrNotFound ) err = r->Create( aKey, aVal );
        delete r;
        return err;
        }
    }

EXPORT_C TInt PhImList( RArray<TPhImEntry>& aOut )
    {
    aOut.Reset();
    TRAPD( err, ListL( aOut ) );
    return err;
    }

EXPORT_C TInt PhImActive( TUint32& aEntryUid )
    {
    TInt v = 0;
    TInt err = GetKey( KPhImKeyActive, v );
    if ( err ) return err;
    if ( v == 0 ) return KErrNotFound;
    aEntryUid = static_cast<TUint32>( v );
    return KErrNone;
    }

EXPORT_C TInt PhImSetActive( TUint32 aEntryUid )
    {
    return SetKey( KPhImKeyActive, static_cast<TInt>( aEntryUid ) );
    }

EXPORT_C TInt PhImResolve( TInt aMode, TUint32& aLayoutUid )
    {
    RArray<TPhImEntry> list;
    TInt err = PhImList( list );
    if ( err ) { list.Close(); return err; }
    TUint32 active = 0;
    PhImActive( active );
    TInt pick = KErrNotFound;
    for ( TInt i = 0; i < list.Count(); ++i )
        {
        if ( list[i].iMode != aMode ) continue;
        if ( list[i].iEntryUid == active ) { pick = i; break; }
        if ( pick < 0 ) pick = i;
        }
    err = KErrNotFound;
    if ( pick >= 0 )
        {
        if ( list[pick].iEntryUid != active ) PhImSetActive( list[pick].iEntryUid );
        // a stock entry with no layout of its own (the Itut one): the caller's stock id stands
        if ( list[pick].iLayoutUid != 0 )
            {
            aLayoutUid = list[pick].iLayoutUid;
            err = KErrNone;
            }
        }
    list.Close();
    return err;
    }

EXPORT_C TInt PhImNext( TPhImEntry& aEntry )
    {
    RArray<TPhImEntry> list;
    TInt err = PhImList( list );
    if ( err == KErrNone && list.Count() == 0 ) err = KErrNotFound;
    if ( err == KErrNone )
        {
        TUint32 active = 0;
        PhImActive( active );
        TInt cur = -1;
        for ( TInt i = 0; i < list.Count(); ++i )
            if ( list[i].iEntryUid == active ) { cur = i; break; }
        aEntry = list[( cur + 1 ) % list.Count()];
        err = PhImSetActive( aEntry.iEntryUid );
        }
    list.Close();
    return err;
    }

EXPORT_C TInt PhImModeOfLayout( TUint32 aLayoutUid, TInt& aMode )
    {
    RArray<TPhImEntry> list;
    TInt err = PhImList( list );
    if ( err == KErrNone )
        {
        err = KErrNotFound;
        for ( TInt i = 0; i < list.Count(); ++i )
            {
            if ( aLayoutUid && list[i].iLayoutUid == aLayoutUid )
                {
                aMode = list[i].iMode;
                err = KErrNone;
                break;
                }
            }
        }
    list.Close();
    return err;
    }

EXPORT_C TInt PhImGetSetting( TUint32 aKey, TInt aDefault, TInt& aValue )
    {
    TInt v = 0;
    TInt err = GetKey( aKey, v );
    aValue = err == KErrNone ? v : aDefault;
    return KErrNone;
    }

EXPORT_C TInt PhImSetSetting( TUint32 aKey, TInt aValue )
    {
    return SetKey( aKey, aValue );
    }
