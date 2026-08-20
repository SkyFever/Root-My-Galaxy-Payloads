# gts7lwifi-T870XXS8DXH1 target status

This directory intentionally has no `target.h`.

Exact stock logs, kernel configuration, symbols, relocations, 4.19 structure
layouts, skb geometry, and the bootloader physical map have been recovered for
SM-T870 firmware
`T870XXS8DXH1`. The original `pselect` input-set route does not cover the stale
4.19 waiter on this build. A macro-gated result-fdset route now covers the exact
legacy waiter location and passes static compilation, but it has not been run
on hardware. Emitting a normal compilable profile before that diagnostic route
is validated would incorrectly suggest that the target is ready for device
execution.

`p0_fingerprint.h` is a passive exact-Image table: 32 slide rows and 256
independently verified source qwords. Its presence does not make this directory
a buildable target.

See [`../../../docs/SM-T870-T870XXS8DXH1.md`](../../../docs/SM-T870-T870XXS8DXH1.md)
for the verified values and the remaining compatibility gates.
