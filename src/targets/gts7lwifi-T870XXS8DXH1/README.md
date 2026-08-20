# gts7lwifi-T870XXS8DXH1 target status

This directory contains a diagnostic-only `target.h` for the exact
`T870XXS8DXH1` stock build. It is not a production/root profile.

Exact stock logs, kernel configuration, symbols, relocations, 4.19 structure
layouts, skb geometry, and the bootloader physical map have been recovered for
SM-T870 firmware
`T870XXS8DXH1`. The original `pselect` input-set route does not cover the stale
4.19 waiter on this build. A macro-gated result-fdset route now covers the exact
legacy waiter location and passes static compilation.

The first hardware run reached the P0 pipe oracle and then panicked in
`_raw_spin_trylock` from `rt_mutex_adjust_prio_chain`. Two later diagnostics
completed page preparation eight times without a reboot, selecting object
indices 12 and 11 with all derived addresses inside verified RAM. The current
build logs all ten legacy waiter words and the task, pi-lock, lock, waiter,
vmemmap parent, and direct-map target ranges. It then exercises the
futex/pselect result route but stops before `sched_setattr`, so the recorded
RT priority-chain path is not entered. The target forces one supervisor
attempt even when the app supplies `EXPLOIT_ATTEMPTS=24`.
The first geometry run safely rejected a checker-only mismatch between the
stack waiter's priority 130 and the reclaimed-page waiter's priority 0. The
route diagnostic compares each field with its own intended constant and logs
both priorities.

`p0_fingerprint.h` is a passive exact-Image table: 32 slide rows and 256
independently verified source qwords.

The safety gates are part of the source:

- builds without `APP_PAYLOAD=1` fail during preprocessing;
- app builds define `APP_SLIDE_DIAGNOSTIC_ONLY=1`;
- app builds require a fresh P0 session;
- the current build requires geometry validation and matching result fd-sets,
  then stops before `sched_setattr`; it never enters the RT priority-chain,
  fops, configfs, or UMH stages.

Build only the app release target:

```sh
make TARGET=gts7lwifi-T870XXS8DXH1 API=33 \
  ANDROID_NDK_HOME=/path/to/android-ndk-r29 release
```

The checked artifact was built with NDK r29 revision `29.0.14206865`:

```text
artifacts/gts7lwifi-T870XXS8DXH1/cve-2026-43499-app-pretrigger-route.so
size: 104128 bytes
SHA-256: cb06149c3a6d5346d39a892b4516fc9802d1b918e09f55c5a4cca62a1b24ff01
```

Post-link inspection found only the slide diagnostic stop marker and no
root/UMH, misc_fops, configfs, or exported root/fops symbol markers.

`support/targets-v3.json` exposes this build to `SM-T870` clients as an
explicit pre-trigger route diagnostic. Its required `kernelsu` download is
a non-root Android stub that exits with status 78; it is not a KernelSU build.
This pairing exists only so the app can launch the diagnostic payload through
the normal schema-v3 selection path. The stub is unreachable in the current
diagnostic stage.

Do not copy another target's header or remove the stop-before-`sched_setattr`
guard until the legacy waiter/result-fdset geometry has been reconciled with
the recorded `_raw_spin_trylock+0x1c` panic.

See [`../../../docs/SM-T870-T870XXS8DXH1.md`](../../../docs/SM-T870-T870XXS8DXH1.md)
for the verified values and the remaining compatibility gates.
