// Copyright The Phorepoc Authors
// SPDX-License-Identifier: EPL-2.0 OR GPL-2.0-or-later

// OURS (PinePhone track, 2026-10-08): HWRM's power-state plug-in for the PinePhone, in place of the adapt.stubs one
// (systemswstubs/hwrmstubplugins/src/PowerStatePlugin.cpp: never notifies, so Avkon's battery pane stayed empty).
// Same class and header (proxy.cpp creates it). Every 10 s it reads the battery from the board's power HAL
// (a64_glue.cpp DA64PowerHal, function 0x5048: fuel gauge %, charging, mV, external power) and reports what changed
// through MHWRMPowerStatePluginCallback -> HWRMServer publishes KPSUidHWRMPowerState -> SysAp's UpdateBatteryBarsL
// -> CAknBatteryNotify (the status pane). A board without that HAL function (x86, virt) answers KErrNotSupported:
// nothing is reported, as with the stub.
#include <e32svr.h>
#include <e32debug.h>
#include <u32std.h>
#include <hwrmpowerstatesdkpskeys.h>
#include <centralrepository.h>
#include "PowerStatePlugin.h"

const TInt KA64PowerHalBattery = 0x5048;
const TInt KA64PowerHalGuardPing = 0x5047;
const TInt KA64PowerHalGuardTimeout = 0x5049;   // a1 = the boot-guard timeout in seconds (a64_glue.cpp UiBootGuardS)
const TInt KPollUs = 10000000;

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
        const TInt r = UserSvr::HalFunction( EHalGroupPower, KA64PowerHalGuardTimeout, (TAny*) secs, 0 );
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
    // the kernel's hang guard (a64_glue.cpp UiGuardTripped, slot B only): "UI up + alive" = WSERV and the homescreen both
    // exist and this server's scheduler still runs (this callback). No ping within 180 s of boot, or none for 60 s later
    // = the watchdog is left to fire -> golden slot A.
    // (the repository may not be up yet in the first seconds: ask again for the first 5 minutes, i.e. every boot's whole guard window)
    if ( self->iKey < 30 )
        SendGuardTimeout();
    const TBool ws = ProcessExists( _L( "\\ewsrv.exe" ) ), hs = ProcessExists( _L( "\\homescreen.exe" ) );
    if ( ws && hs )
        UserSvr::HalFunction( EHalGroupPower, KA64PowerHalGuardPing, 0, 0 );
    if ( ++self->iKey <= 20 )
        RDebug::Printf( "PH: guard ping check wserv=%d homescreen=%d", ws, hs );
    TInt b[4] = { -1, 0, 0, 0 };
    TPckg<TInt[4]> pk( b );
    const TInt r = UserSvr::HalFunction( EHalGroupPower, KA64PowerHalBattery, &pk, 0 );
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
