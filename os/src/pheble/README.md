# pheble: Bluetooth LE

Bluetooth LE for Phorepoc on Apache NimBLE (`../../third_party/nimble`, 1.9.0). Central role and GATT client,
SMP with Secure Connections (Just Works / numeric comparison) and bonding.

| Part | What |
|---|---|
| `router/` `pheblerouter.dll` | HCTL plug-in (impl 0xE7015E11) that wraps the board's HCTL: LE commands, LE Meta events, the events and ACL of LE handles go to NimBLE; BR/EDR stays with the 2010 stack. Merges the event masks, shares the ACL buffers. |
| `host/` `pheblehost.dll` | NimBLE + the Symbian NPL (`port/`) + the bond store (`pheble_bonds.dat` in the stack process's private dir) + the `!PheBle` server. Loaded by the router into the Bluetooth stack's process. |
| `client/` `pheble.dll` | `RPheBle` (`inc/pheble.h`). |
| `test/` `phebletest.exe` | LE scan test: logs named advertisers, PASS when a "Pebble ..." is seen. |
| `data/pheblerouter.ini` | template for a board's `z:\private\101f7989\bluetooth\pheblerouter.ini`. |

A board turns BLE on with two files in its zdata: `corehci_symbian.ini` `hctl_uid= 0xE7015E11` and `pheblerouter.ini`
`inner_hctl_uid=` its own HCTL. Boards without it keep their HCTL and never load the router.

## Configuration (changes against NimBLE's Linux syscfg.h)
BLE_TRANSPORT_LL custom (not socket); BLE_ROLE_PERIPHERAL/BROADCASTER 0; BLE_SM_BONDING 1, BLE_SM_SC 1, BLE_SM_LEGACY 1,
BLE_SM_MITM 0, BLE_SM_IO_CAP DISPLAY_YESNO, key distribution ENC|ID both ways; BLE_MAX_CONNECTIONS 2; BLE_VERSION 42;
BLE_ATT_PREFERRED_MTU 339; BLE_TRANSPORT_ACL_SIZE 1024; BLE_TRANSPORT_EVT_SIZE 257; BLE_STORE_MAX_BONDS 8,
MAX_CCCDS 16; MSYS_1_BLOCK_COUNT 64; BLE_GATT_MAX_PROCS 8; BLE_HS_LOG_LVL 2; BLE_HS_AUTO_START 0.

The design and the progress live in the private planning notes (ble-plan); the routing rules are in the header of
`router/pheblerouter.cpp`.
