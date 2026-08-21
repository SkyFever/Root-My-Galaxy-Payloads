# Galaxy Tab S7 SM-T870 / T870XXS8DXH1 port record

## Status

```text
model: SM-T870
device/product: gts7lwifi / gts7lwifixx
display build: TP1A.220624.014.T870XXS8DXH1
fingerprint: samsung/gts7lwifixx/gts7lwifi:13/TP1A.220624.014/T870XXS8DXH1:user/release-keys
Android SDK: 33
ABI: arm64-v8a
page size: 4096
kernel release: 4.19.113-27114284
kernel build: #1 SMP PREEMPT Mon Aug 5 15:36:53 +07 2024
```

The exploit profile, app payload, KernelSU module, and late-load binary are
derived from this exact firmware and are build- and static-analysis verified.
Hardware execution remains pending. The implementation follows
[`PORTING.md`](PORTING.md) and keeps the NebuSec/Root My Galaxy exploit flow;
only target-gated 4.19 compatibility is added.

## Stock inputs and address space

| Object | Size | SHA-256 |
| --- | ---: | --- |
| stock `boot.img` | retained locally | `F555FF2BB68180A49D4F5617946617F2728A22E536B863A4F2FD31933A4F7D72` |
| raw ARM64 `Image` | 55,459,852 | `8E2626AC81C0C857617BB870C92A80EF98987174CEF02D6EDB68E4708E4D603E` |
| recovered `vmlinux.elf` | retained locally | `AB58BD429E60EE3B52795BDCED1CB0A6960C28BA3BC06722D44E82DD95620ADC` |

The matching Samsung Open Source Center release is retained locally as
`SM-T870_EUR_13_Opensource`. The Image header has `text_offset=0x80000`.
The exact source, stock configuration, Image, recovered symbols and read-only
device data establish:

```text
KIMAGE_TEXT_BASE:    0xffffff8008080000
P0_PHYS_OFFSET:      0x80000000
P0_KERNEL_PHYS_LOAD: 0x80080000
P0_PAGE_OFFSET:      0xffffffc000000000
DIRECT_MAP_END:      0xffffffc300000000
VMEMMAP_START:       0xffffffbf00000000
```

The runtime trace event ID for `sched_blocked_reason` is 72. The worker caller
is the instruction after the blocking `worker_thread -> schedule` call at
Image offset `0x0005e7c4`.

## 4.19 exploit compatibility

Exact source DWARF gives a 0x50-byte legacy `rt_mutex_waiter`. The stale waiter
starts at syscall-entry stack pointer `S-0x190`; the `pselect6` result fd-set
starts at `S-0x198`, so the waiter begins at qword shift 1. The T870-only
result-route branch waits until `core_sys_select` has materialized those result
sets, then lets the existing consumer issue `sched_setattr` before the same
thread enters another syscall.

This kernel's configfs file operations use legacy `.read/.write` callbacks.
The T870-only `LEGACY_CONFIGFS_FILE_RW` branch therefore places the exact
`configfs_read_file` and `configfs_write_bin_file` addresses in those slots.
All existing targets retain the `.read_iter/.write_iter` layout.

The C layout size of `mm_struct` is 0x3c0, while the connected device reports
the actual SLUB object size as 0x400, 32 objects per slab, order 3. The exploit
uses the runtime cache geometry (`MM_STRUCT_SZ=0x400`, `MM_ORDER=3`). The
target's workqueue offsets are `dfl_pwq=0xc0`, `worklist=0x28`, and
`nr_idle=0x3c`; these replace values from the discarded experimental port.

`p0_fingerprint.h` was generated with the repository's official script for
all 32 candidates from `0x000000` through `0x1f0000`. All 256 source qwords
were read back from the exact stock Image and matched.

## Root handoff and locked-stock policy

The connected stock device reports verified boot `green`, flash lock `1`, and
warranty bit `0`. Its configuration has `CONFIG_MODULE_SIG_FORCE=y`, and
Samsung DEFEX remains active while the boot state is locked. The target header
therefore defines the exact stock symbols:

```text
sig_enforce:         0xffffff800b24a980
boot_state_unlocked: 0xffffff800a996381
```

Immediately before the repository's existing UMH handoff, and only when these
target macros are present, `root.c` clears `sig_enforce` and sets
`boot_state_unlocked` through the already established physical kernel-write
primitive. No new exploit or root path is introduced.

## KernelSU 4.19 port

KernelSU is built from v3.2.5 commit
`b0bc817b4e966aa6aa830834eaf6ef765d821d40` with the repository's Samsung
KDP/RKP/DEFEX patch. For Linux 4.19, the compatibility layer follows the exact
stock APIs:

- native stock `commit_creds()` and `put_cred()` retain Samsung KDP handling;
- DEFEX credential getters/setters are resolved from kallsyms, while the
  payload's exact boot-state write disables enforcement before module load;
- `CONFIG_KPROBES=n` uses the stock syscall tracepoints for KernelSU's original
  reboot-magic Manager FD delivery;
- the old SELinux globals `selinux_ss`, `selinux_avc`, and
  `selinux_enforcing` replace the newer `selinux_state` interface;
- legacy unmount uses the target's `ksys_umount()` with the 4.19 address-limit
  sequence;
- KMI detection falls back to `legacy-4.19`, selecting the embedded
  `legacy-4.19_kernelsu.ko` asset.

The module reports exact vermagic:

```text
4.19.113-27114284 SMP preempt mod_unload modversions aarch64
```

The manual-relocation audit reports 183 undefined imports, zero missing target
symbols, zero module version entries, and zero CRC mismatches. Forty-six
imports are intentionally resolved from target kallsyms rather than the export
table. No newer KDP helper or kprobe symbol remains undefined.

## Published build outputs

| File | Size | SHA-256 |
| --- | ---: | --- |
| `artifacts/gts7lwifi-T870XXS8DXH1/cve-2026-43499-app.so` | 104,128 | `36C837AD9276D4757D2F330D25CDF7C45945D1942F268262A1E826DE6E14C1E5` |
| `kernelsu/legacy-4.19_kernelsu-gts7lwifi-T870XXS8DXH1-kdp.ko` | 208,368 | `84C44917D566CAF1BE60EF7E6400ABE2065117051B872D790B62DE76741EDD3A` |
| `kernelsu/ksud-gts7lwifi-T870XXS8DXH1-kdp` | 4,731,432 | `E0F450ACEA5B948B5F8A5B7013666E1C3CD98DD40FA178057E493ABD9C8E458E` |

The Android 33 AArch64 `ksud` embeds the stripped KO under the exact asset name
`legacy-4.19_kernelsu.ko`. The standalone KO is retained for audit. Both must
always be rebuilt and published together.

## Validation boundary

The payload release build, an existing A155 regression release build, KernelSU
module build, module symbol audit, Rust compile check, formatting check, and
release `ksud` build pass. Clippy reaches two pre-existing unrelated warnings
in `userspace/ksud/src/module.rs`; that file is not changed by this port.

The support profile is intentionally exact to model `SM-T870` and kernel
version `4.19.113`. Device execution is the next validation step; until it
succeeds, this record must continue to say hardware-untested.
