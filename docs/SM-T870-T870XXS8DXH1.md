# SM-T870 / T870XXS8DXH1 port

This record follows `docs/PORTING.md` for the Galaxy Tab S7 Wi-Fi European
Android 13 release. Values in this file are tied to this firmware and must not
be reused for another T870 build.

## 1. Exact target identity

```text
model: SM-T870
device: gts7lwifi
region: EUR
display build: TP1A.220624.014.T870XXS8DXH1
AP/PDA: T870XXS8DXH1
kernel release: 4.19.113-27114284
kernel build: #1 SMP PREEMPT Mon Aug 5 15:36:53 +07 2024
```

The matching Samsung Open Source Release Center tree is unpacked at
`common/SM-T870_EUR_13_Opensource`. Its kernel `Makefile` reports 4.19.113 and
the exact product configuration is
`arch/arm64/configs/vendor/gts7lwifi_eur_open_defconfig`.

## 2. Stock kernel provenance

```text
boot.img size: 71303168
boot.img SHA-256: f555ff2bb68180a49d4f5617946617f2728a22e536b863a4f2fd31933a4f7d72
raw kernel size: 55459852
raw kernel SHA-256: 8e2626ac81c0c857617bb870c92a80ef98987174cef02d6edb68e4708e4d603e
stock config size: 190405
stock config SHA-256: 6664ed6b19c847ea656fbe5aa4231f5b5a68f80891848c3c56acdecf719a9aa1
```

The recovered ELF has `_text = 0xffffff8008080000`. The generated exact
configuration matches the stock configuration except for compiler-version
metadata, and `LOCALVERSION=-27114284` produces the exact target release.

## 3. Symbols and exploit target

The no-BTF image was reconstructed with `vmlinux-to-elf`. Target constants are
kept in:

```text
src/targets/gts7lwifi-T870XXS8DXH1/target.h
src/targets/gts7lwifi-T870XXS8DXH1/p0_fingerprint.h
```

Important exact values include:

```text
KIMAGE_TEXT_BASE: 0xffffff8008080000
P0_PHYS_OFFSET: 0x80000000
SLIDE_TRACEFS_EVENT_ID: 72
SLIDE_TRACEFS_WORKER_CALLER_OFF: 0x0005e7c4
SLIDE_PSELECT_WORD_SHIFT: 1
SELINUX_ENFORCING_OFF: 0x0292a200
MODULE_SIG_ENFORCE_OFF: 0x031ca980
```

The stock byte at `sig_enforce` is 1. The app payload changes this data byte to
0 before the privileged helper invokes KernelSU's late loader; it does not
patch kernel text.

The currently published checkpoint-instrumented production exploit is fixed
at 104128 bytes:

```text
artifacts/gts7lwifi-T870XXS8DXH1/cve-2026-43499-app.so
SHA-256: 1cc3b9245cf57572e177d8911a81fe1037e4f76f65e27da39e4c171ba0501eac
```

### KernelSnitch page-prepare diagnostic

The 2026-08-21 12:03 hardware run stopped immediately after the
`KernelSnitch profile` line. The next boot reported Samsung reset reason
`KPON` and DropBox tag
`SYSTEM_LAST_KMSG_6_20260821_120502_KP`, confirming a kernel panic rather
than an app timeout. That run produced no pselect, P0 pipe-gate, KASLR, or
KernelSU line, so it does not test the in-kernel pselect route.

The follow-up page-prepare diagnostic completed seven distinct hardware
attempts through collision search, mm-struct brute-force, payload construction,
and skb reclaim. Every completed attempt leaked object index 8 and sent all
16/16 reclaim skbs before the diagnostic stop. The eighth attempt was cut off
by the app supervisor's 120-second overall timeout; the device did not reboot.

`DEFAULT_EXPLOIT_ATTEMPTS=1` controls the payload-side default but does not
override the app's external `attempts=24` supervisor setting. The diagnostic
therefore repeated until the outer timeout. This result validates the complete
page-prepare path on hardware. The stop and verbose target flags are removed,
the guarded checkpoints remain available in shared source, and the published
artifact has returned to the in-kernel pselect/P0 production route.

The 2026-08-21 12:53 production rerun again stopped immediately after the
`KernelSnitch profile` line and rebooted with `KPON`. The next boot stored
`SYSTEM_LAST_KMSG_7_20260821_125437_KP`. Its retained pstore tail begins after
Samsung's truncation marker and contains the panic-time idle-CPU dumps, but
not the initiating watchdog line or the originally stuck CPU stack. It
therefore cannot distinguish a page-prepare lockup from a failure immediately
after page preparation and before the first pselect-route log.

The next published artifact enables only
`APP_KERNEL_PAGE_DIAGNOSTIC_CHECKPOINTS`. It does not enable
`KERNELSNITCH_VERBOSE`, does not stop after page preparation, and does not
change the KernelSnitch profile or any pselect/rt_mutex timing. Its build label
is `gts7lwifi-T870XXS8DXH1-app-ks-collision-checkpoint`. The same flag exposes
the existing P0 pipe-page and `prepare_kernel_page()` boundaries, plus
baseline, waiter-pileup, collision-scan, and waiter-decrease phases inside the
existing KernelSnitch collision routine. No timed measurement or exploit
behavior is changed.

### Exact legacy pselect result route

The first result-route hardware runs reached the legacy pselect route on every
recorded attempt, and `sched_setattr` returned zero, but the P0 pipe oracle
reported `hits=0 changed=0`. Those runs predated the T870-specific correction
to the PI-requeue ordering. They do not validate the later in-kernel route and
do not test the corrected PI ordering together with the result route.

The exact stock ELF fixes both the overlap and the required execution window:

```text
__arm64_sys_futex frame:       0x070
do_futex frame:                0x1e0
rt_mutex_waiter in do_futex:   sp + 0x0c0
stale waiter from syscall SP:  -0x190

__arm64_sys_pselect6 frame:    0x0a0
core_sys_select frame:         0x1c0
stack fd-set base:             syscall SP - 0x210
result input fd-set base:      syscall SP - 0x198
waiter word zero:              result input qword 1
```

This independently confirms `SLIDE_PSELECT_WORD_SHIFT=1`, but it also rejects
the in-kernel input route. With `nfds=320`, the three input sets occupy qwords
0 through 14, while the stale waiter starts at qword 16. `core_sys_select()`
zeros the three result sets before entering `do_select()`, so triggering while
pselect is blocked presents a zero waiter lock and can fault in
`_raw_spin_trylock()`.

The T870 payload therefore uses the existing `SLIDE_PSELECT_RESULT_ROUTE` and
its null timeout. `/dev/null` makes every selected read/write descriptor ready;
after `pselect6` returns, byte-for-byte matching result masks prove that all ten
legacy waiter qwords were materialized in the overlapping result sets. The
pselect thread then stays in userspace while the consumer invokes
`sched_setattr`. The T870-only upstream PI-requeue order and owner-acquired
synchronization remain enabled.

The same exact defconfig was compiled with debug information to audit the
remaining legacy geometry. It confirms the checked-in `rt_mutex_waiter`,
`rt_mutex`, `task_struct`, `struct page`, `pipe_buffer`, PAGE_OFFSET and
VMEMMAP values. `sizeof(struct skb_shared_info) == 0x150`; aligned to the
64-byte cache line this is `0x180`, so `SKB_MAX_HEAD(0) == 0xe80` and the
checked-in `SKB_DATA_DELTA=-0xe80` is also retained.

## 4. Exact KernelSU 4.19 build

The module is based on KernelSU v3.2.5 plus
`KernelSU-v3.2.5-samsung-kdp-rkp-defex.patch`, followed by
`KernelSU-v3.2.5-samsung-4.19-t870.patch`. It reports:

```text
vermagic: 4.19.113-27114284 SMP preempt mod_unload modversions aarch64
```

The stock configuration has `CONFIG_MODVERSIONS=y`,
`CONFIG_MODULE_SIG_FORCE=y`, and no `CONFIG_KPROBES` or
`CONFIG_KRETPROBES`. Therefore this port:

- keeps live text and syscall-table patching disabled;
- installs the KernelSU reboot control FD through the existing raw syscall
  tracepoint instead of a reboot kprobe;
- keeps Samsung KDP and DEFEX shadow-credential synchronization, while omitting
  the unavailable DEFEX kprobe bypass;
- uses a zero-length `__versions` section and retains `.symtab`/`.strtab` for
  KernelSU's manual-relocation loader.

Samsung places `selinux_state` in `.bss.rtic`. The recovered stock table shows
it only as undefined, although `selinux_init` resolves its runtime address to
`_text + 0x047cf000`. The target-specific ksud applies that fallback only when
`/proc/sys/kernel/osrelease` is exactly `4.19.113-27114284`.

The recovered target `Module.symvers` contains 12766 unique exports:

```text
size: 717023
SHA-256: 9a15e9b185f3bf4e28dd3c6a075fa11abfbcc848b60f78ce5b905cbb683d4be9
```

The stripped module audit reports 176 undefined imports, zero module-version
entries, 40 symbols intentionally resolved from kallsyms, zero CRC mismatches,
and one target-derived fallback (`selinux_state`). It has no `register_kprobe`,
`register_kretprobe`, or `stop_machine` import.

Published KernelSU artifacts:

```text
kernelsu/android13-4.19.113_kernelsu-gts7lwifi-T870XXS8DXH1-kdp.ko
size: 208616
SHA-256: 728d5247e355b1b6f7d393613270a3eacee6b394ef6f84e6fba4f00f8700eb10

kernelsu/ksud-gts7lwifi-T870XXS8DXH1-kdp
size: 3557112
SHA-256: 3f4908039eb62123c395e5f8b4456df5a88856121e09191c5be1c6ce5159e5f8
```

The ksud build maps the exact legacy release to the embedded asset
`4.19.113-27114284_kernelsu.ko` and reports userspace version
`32525 / 3.2.5-t870`.

## 5. Support feed and hardware gate

`support/targets-v3.json` selects this payload only for `SM-T870` with the
leading kernel version `4.19.113`. Both artifact URLs are repository-relative,
and `requiresFreshP0Session` is enabled.

This pair is build-verified but not yet hardware-verified. The first device run
must confirm, in order:

1. exploit summary reports physical read/write, KASLR, and root helper success;
2. late loader selects `4.19.113-27114284_kernelsu.ko`;
3. the log shows the target-specific `selinux_state` resolution;
4. `init_module` succeeds and the reboot tracepoint registers;
5. the KernelSU control check reports version 32525 with LKM and late-load
   flags;
6. a KernelSU root shell can read `dmesg`, all logcat buffers, and pstore paths.

Do not mark the pair device-tested until all six checks pass on the exact
`T870XXS8DXH1` stock kernel.
