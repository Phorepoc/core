// Copyright The Phorepoc Authors
// SPDX-License-Identifier: EPL-2.0 OR GPL-2.0-or-later

// OURS (2026-10-08): HWRM's power-state plug-in, in place of the adapt.stubs one
// (systemswstubs/hwrmstubplugins/src/PowerStatePlugin.cpp: never notifies, so Avkon's battery pane stayed empty).
// Same class and header (proxy.cpp creates it). Every second it reads the battery from the board's power HAL
// (phhal.h EPhHalPowerBattery: fuel gauge %, charging, mV, external power) and reports what changed
// through MHWRMPowerStatePluginCallback -> HWRMServer publishes KPSUidHWRMPowerState -> SysAp's UpdateBatteryBarsL
// -> CAknBatteryNotify (the status pane). A board without a battery answers KErrNotSupported: nothing is reported,
// as with the stub.
#include <e32svr.h>
#include <e32debug.h>
#include <u32std.h>
#include <hwrmpowerstatesdkpskeys.h>
#include <centralrepository.h>
#include "PowerStatePlugin.h"
#include "../phorepoc/inc/phhal.h"
#include "../phedevmenu/pheshells.h"

// The battery is read every second (a HAL call that copies the kernel's last reading: cheap); a change of the charger shows
// within a second. The guard ping / boot-guard timeout / process checks keep their old 10 s cadence (KGuardEvery polls).
const TInt KPollUs = 1000000;
const TInt KGuardEvery = 10;

// the developer menu's "Boot guard timeout" (CenRep 0xE7015A01 key 3, src/phedevmenu/phedevmenu.h) -> the kernel's boot guard.
// No repository / no key (or a board without the HAL function) = nothing is sent: the kernel keeps its built-in default.
static void SendGuardTimeout()
    {
    CRepository* repo = NULL;
    TRAPD( err, repo = CRepository::NewL( TUid::Uid( 0xE7015A01 ) ) );
    if ( err != KErrNone )
        return;
    TInt secs = 0;
    if ( repo->Get( 3, secs ) == KErrNone )
        {
        const TInt r = UserSvr::HalFunction( EHalGroupPower, EPhHalPowerGuardTimeout, (TAny*) secs, 0 );
        if ( r != KErrNotSupported )
            RDebug::Printf( "PH: boot guard timeout %d s (developer menu key 3) -> %d", secs, r );
        }
    delete repo;
    }

// matched on the EXE file name (a process can rename itself: homescreen.exe's process name does not start with
// "homescreen", which kept the guard ping from ever being sent -- every slot-B boot fell back at ~195 s)
static TBool ProcessExists( const TDesC& aExe )
    {
    TFindProcess fp( _L( "*" ) );
    TFullName n;
    while ( fp.Next( n ) == KErrNone )
        {
        RProcess p;
        if ( p.Open( fp ) != KErrNone )
            continue;
        const TBool hit = p.FileName().Right( aExe.Length() ).CompareF( aExe ) == 0;
        p.Close();
        if ( hit )
            return ETrue;
        }
    return EFalse;
    }

// The home of the shell that runs: S^3's homescreen.exe, or -- for a shell a package installed (pheshells.h: Etesian, Orbit)
// -- that shell's launcher, which "lives as long as the shell does" (pheshells.h). Without the second case the guard never
// pinged under such a shell and reset it to golden slot A after 180 s. Read on each guard tick (10 s): no state of our own
// (a plug-in DLL has no writable static data; the class is the stub's).
static void CloseShells( TAny* aShells )
    {
    PheShellsClose( *static_cast<RPheShells*>( aShells ) );
    }

static TBool SelectedShellLauncherExistsL()
    {
    CRepository* repo = CRepository::NewL( TUid::Uid( 0xE7015A01 ) );
    CleanupStack::PushL( repo );
    HBufC* id = PheSelectedShellIdL( repo );
    CleanupStack::PopAndDestroy( repo );
    CleanupStack::PushL( id );
    TBool hit = EFalse;
    if ( id->CompareF( KPheShellIdS3 ) != 0 )
        {
        RFs fs;
        User::LeaveIfError( fs.Connect() );
        CleanupClosePushL( fs );
        RPheShells shells;
        CleanupStack::PushL( TCleanupItem( CloseShells, &shells ) );
        PheListShellsL( fs, shells );
        for ( TInt i = 0; i < shells.Count() && !hit; ++i )
            {
            if ( shells[i]->Id().CompareF( *id ) != 0 || !shells[i]->Launcher().Length() )
                continue;
            TFileName exe( _L( "\\" ) );
            exe.Append( TParsePtrC( shells[i]->Launcher() ).NameAndExt() );
            hit = ProcessExists( exe );
            }
        CleanupStack::PopAndDestroy( 2, &fs );
        }
    CleanupStack::PopAndDestroy( id );
    return hit;
    }

static TBool ShellHomeExists()
    {
    if ( ProcessExists( _L( "\\homescreen.exe" ) ) )
        return ETrue;
    TBool hit = EFalse;
    TRAP_IGNORE( hit = SelectedShellLauncherExistsL() );
    return hit;
    }
// (a plug-in DLL has no writable static data: what was last reported = iKeyValue, a byte per key, value + 16; 0 = none)

CPowerStatePlugin* CPowerStatePlugin::NewL()
    {
    CPowerStatePlugin* self = new(ELeave) CPowerStatePlugin();
    CleanupStack::PushL(self);
    self->ConstructL();
    CleanupStack::Pop();
    return self;
    }

CPowerStatePlugin::~CPowerStatePlugin()
    {
    delete iPeriodic;
    }

CPowerStatePlugin::CPowerStatePlugin() : iKey( 0 ), iKeyValue( 0 )
    {
    }

void CPowerStatePlugin::ConstructL()
    {
    iPeriodic = CPeriodic::NewL( CActive::EPriorityStandard );
    iPeriodic->Start( 1000000, KPollUs, TCallBack( CPowerStatePlugin::Callback, this ) );
    }

static void Report( MHWRMPowerStatePluginCallback* aCb, TUint32 aKey, TInt aValue, TInt& aLast )
    {
    const TInt shift = 8 * ( aKey - 1 );          // KHWRMBatteryLevel 1, KHWRMBatteryStatus 2, KHWRMChargingStatus 3
    const TInt enc = ( aValue + 16 ) & 0xFF;
    if ( ( ( aLast >> shift ) & 0xFF ) == enc || !aCb )
        return;
    aLast = ( aLast & ~( 0xFF << shift ) ) | ( enc << shift );
    TRAP_IGNORE( aCb->NotifyStateChange( aKey, aValue ) );
    }

TInt CPowerStatePlugin::Callback( TAny* aPtr )
    {
    CPowerStatePlugin* self = (CPowerStatePlugin*) aPtr;
    // the kernel's hang guard (phhal.h EPhHalPowerGuardPing: a board that has one): "UI up + alive" = WSERV and the homescreen both
    // exist and this server's scheduler still runs (this callback). No ping within 180 s of boot, or none for 60 s later
    // = the watchdog is left to fire -> golden slot A.
    // (the repository may not be up yet in the first seconds: ask again for the first 5 minutes, i.e. every boot's whole guard window)
    // This callback runs every KPollUs (1 s: the charging indicator follows the cable within a second); the guard work below is
    // the old 10 s cadence, iKey counts the callbacks.
    const TBool guardTick = ( self->iKey % KGuardEvery ) == 0;
    if ( guardTick && self->iKey < 30 * KGuardEvery )
        SendGuardTimeout();
    if ( guardTick )
        {
        const TBool ws = ProcessExists( _L( "\\ewsrv.exe" ) ), hs = ShellHomeExists();
        if ( ws && hs )
            UserSvr::HalFunction( EHalGroupPower, EPhHalPowerGuardPing, 0, 0 );
        if ( self->iKey <= 20 * KGuardEvery )
            RDebug::Printf( "PH: guard ping check wserv=%d homescreen=%d", ws, hs );
        }
    ++self->iKey;
    TInt b[4] = { -1, 0, 0, 0 };
    TPckg<TInt[4]> pk( b );
    const TInt r = UserSvr::HalFunction( EHalGroupPower, EPhHalPowerBattery, &pk, 0 );
    if ( r == KErrNotSupported )
        self->iPeriodic->Cancel();          // no such board function: stay silent, as the stub
    if ( r != KErrNone )
        return KErrNone;                    // (KErrNotReady = no reading yet: ask again next time)
    const TInt pct = b[0];
    // 8 bars (EBatteryLevelLevel0..7); no valid gauge yet -> from the voltage (3.3 V empty .. 4.2 V full)
    TInt p = pct;
    if ( p < 0 )
        p = b[2] <= 3300 ? 0 : b[2] >= 4200 ? 100 : ( b[2] - 3300 ) * 100 / 900;
    const TInt level = EBatteryLevelLevel0 + ( p * 8 ) / 100;
    const TInt status = p <= 3 ? EBatteryStatusEmpty : p <= 12 ? EBatteryStatusLow : EBatteryStatusOk;
    const TInt charging = b[1] ? ( p >= 100 ? EChargingStatusChargingComplete : EChargingStatusCharging )
        : b[3] ? ( p >= 95 ? EChargingStatusChargingComplete : EChargingStatusNotCharging )
        : EChargingStatusNotConnected;
    Report( self->iNotificationCallback, KHWRMBatteryLevel, level > EBatteryLevelLevel7 ? EBatteryLevelLevel7 : level, self->iKeyValue );
    Report( self->iNotificationCallback, KHWRMBatteryStatus, status, self->iKeyValue );
    Report( self->iNotificationCallback, KHWRMChargingStatus, charging, self->iKeyValue );
    return KErrNone;
    }

void CPowerStatePlugin::IncrementKeyAndValue()
    {
    }
