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
`_raw_spin_trylock` from `rt_mutex_adjust_prio_chain`. The current build is a
safer page-preparation diagnostic: it enables the fresh-session path, logs
KernelSnitch/reclaim checkpoints, and stops before the RT-mutex trigger. The
second hardware run completed this stage seven times without a reboot, always
selecting object index 12 with all derived addresses inside verified RAM. The
target now forces one supervisor attempt even when the app supplies
`EXPLOIT_ATTEMPTS=24`.

`p0_fingerprint.h` is a passive exact-Image table: 32 slide rows and 256
independently verified source qwords.

The safety gates are part of the source:

- builds without `APP_PAYLOAD=1` fail during preprocessing;
- app builds define `APP_SLIDE_DIAGNOSTIC_ONLY=1`;
- app builds require a fresh P0 session;
- the current build stops after kernel-page preparation and never enters the
  RT-mutex trigger, fops, configfs, or UMH code.

Build only the app release target:

```sh
make TARGET=gts7lwifi-T870XXS8DXH1 API=33 \
  ANDROID_NDK_HOME=/path/to/android-ndk-r29 release
```

The checked artifact was built with NDK r29 revision `29.0.14206865`:

```text
artifacts/gts7lwifi-T870XXS8DXH1/cve-2026-43499-app-page-prepare-single.so
size: 104128 bytes
SHA-256: 38d527317c5bfb62bc3089a4caba3c4724122bbec1145629bbb022671396d524
```

Post-link inspection found only the slide diagnostic stop marker and no
root/UMH, misc_fops, configfs, or exported root/fops symbol markers.

`support/targets-v3.json` exposes this build to `SM-T870` clients as an
explicit page-prepare diagnostic profile. Its required `kernelsu` download is
a non-root Android stub that exits with status 78; it is not a KernelSU build.
This pairing exists only so the app can launch the diagnostic payload through
the normal schema-v3 selection path. The stub is unreachable in the current
diagnostic stage.

Do not copy another target's header or remove the page-prepare stop until the
legacy waiter/result-fdset priority-chain geometry has been reconciled with the
recorded `_raw_spin_trylock+0x1c` panic.

See [`../../../docs/SM-T870-T870XXS8DXH1.md`](../../../docs/SM-T870-T870XXS8DXH1.md)
for the verified values and the remaining compatibility gates.
