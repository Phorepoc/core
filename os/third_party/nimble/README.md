# Apache NimBLE (vendored)

- Upstream: https://github.com/apache/mynewt-nimble, tag `nimble_1_9_0_tag` (commit
  `da7e3256da3ba80b232df880f40d8359311cc62e`). Licence Apache-2.0 (`LICENSE`, `NOTICE`); TinyCrypt under its own
  licence (`ext/tinycrypt/LICENSE`).
- Used by `src/pheble` (the Phorepoc Bluetooth LE host): the host, host/util, the GAP and GATT services, the RAM store,
  the transport core (`nimble/transport/src/transport.c`), `porting/nimble` and TinyCrypt (LE Secure Connections).
- Copied unchanged. Left out: the controller, mesh, audio, apps, other transports, the newt build files (`*.yml`),
  tests, `nimble/host/store/config`, and `porting/nimble/include/syscfg` (Phorepoc's own configuration replaces it:
  `src/pheble/port/include/syscfg/syscfg.h`). The Symbian porting layer is ours: `src/pheble/port`.
- To update: copy the same directories from a newer tag, re-apply the syscfg changes listed in `src/pheble/README.md`.
