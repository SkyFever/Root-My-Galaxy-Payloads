# Galaxy Tab S7 SM-T870 / T870XXS8DXH1 port record

This record freezes the exact device, firmware, kernel, and offline analysis
inputs for profile `gts7lwifi-T870XXS8DXH1`. It remains a diagnostic-only
target while hardware validation is in progress. No partition or boot image
was modified while collecting evidence or running the app payload.

## Status

```text
device/log capture: complete
exact stock boot image: verified
IKCONFIG extraction: complete
symbol recovery: complete
4.19 structure analysis: offline offsets complete
pselect input-set route: incompatible with this build's stack geometry
pselect result-set route: macro-gated prototype, static compile passed
physical map / P0 table: offline proof complete
target.h: diagnostic-only, static compile passed
diagnostic artifact: NDK r29 / Android API 33 build complete
hardware exploit validation: page preparation stable; RT-mutex trigger disabled
```

The vulnerable `remove_waiter()` implementation is present in this 4.19
kernel lineage, but vulnerability presence alone does not make the repository's
existing 5.10+ profiles safe to reuse. A macro-gated 4.19 result-fdset route
has now been added to the shared code. The remaining gate is deliberately a
diagnostic-only hardware validation of that route. The target header rejects
non-app builds, and the app returns immediately after the slide diagnostic;
the fops, configfs, and root/UMH stages are unreachable in this build.

## First hardware result

The first app run on 2026-08-20 matched the exact support profile and reached
`p0 pipe oracle prepared base=ffffffc297858000`. The device then rebooted
before another payload line was durably captured.

Samsung's reset records identify a kernel panic followed by a non-secure
watchdog reset. The recorded PC was `_raw_spin_trylock+0x1c` and the LR was
`rt_mutex_adjust_prio_chain+0x2d4`. In the exact stock image that PC is
`ldr w8, [x0]`, so the RT priority chain attempted to lock an invalid address.
The KernelSU diagnostic stub was never invoked.

The next artifact therefore enables the compile-time fresh-P0 path that the
support JSON already required, emits KernelSnitch and reclaim checkpoints, and
returns immediately after kernel-page preparation. It does not enter the
pselect/RT-mutex write trigger.

## Second hardware result

The page-preparation diagnostic run captured in
`RootMyGalaxy-20260820-192711-failed.log.txt` did not reboot or panic the
device. Seven complete preparation cycles reached the deliberate
`rt_mutex trigger not entered` stop before the app deadline interrupted the
eighth cycle. Every completed cycle had all of the following properties:

```text
KernelSnitch collision confirmations: 3
reclaim messages sent: 16/16
leaked mm_struct object_index: 12
RT-mutex trigger: not entered
```

The seven leaked order-3 bases and their derived fake-task/fake-lock addresses
all translate into the verified physical RAM segments. The repeated object
index and successful reclaim make the page-selection stage stable enough to
pass its current hardware gate. This also narrows the first panic away from a
simple out-of-RAM KernelSnitch candidate and toward the legacy waiter/result
fd-set priority-chain geometry or its state at trigger time.

The app supplied `EXPLOIT_ATTEMPTS=24`, which overrode the target's default of
one and caused the safe diagnostic to repeat until the app timeout. The new
artifact uses a target-only compile-time override that forces the supervisor
to one attempt even when the app supplies that environment value. The
RT-mutex trigger remains disabled.

## Third hardware result

The forced-single-attempt run captured in
`RootMyGalaxy-20260820-194326-failed.log.txt` selected the new payload ID and
logged `requested=24 effective=1`, followed by exactly one supervisor attempt.
The apparent second copy in the file is the app embedding the same exploit log
inside its final error text: both copies have PID 29712/29713 and identical
addresses. It is not a second execution.

The run completed page preparation in 8.344 seconds with collision
confirmations 3, reclaim 16/16, and object index 11:

```text
pipe direct map:  ffffffc1ae090000 -> physical 0x22e090000 (RAM 2)
page direct map:  ffffffc060ff8000 -> physical 0x0e0ff8000 (RAM 1)
fake task:        ffffffc060ff8180 -> physical 0x0e0ff8180 (RAM 1)
fake lock:        ffffffc060ffc380 -> physical 0x0e0ffc380 (RAM 1)
```

The device did not reboot or panic, and the run reached the deliberate
`rt_mutex trigger not entered` stop. Object index 11, after seven earlier
index-12 successes, shows that page preparation is stable without depending
on one fixed object position.

Exact stock disassembly further narrows the first panic. At
`remove_waiter+0x158`, the kernel loads `next_lock` from offset `0x38` of the
task's blocked waiter and passes it as argument `x3`. At
`rt_mutex_adjust_prio_chain+0x2cc`, that value is moved to `x0`; the call at
`+0x2d4` enters `_raw_spin_trylock`, where the fault occurred. The next
diagnostic therefore validates legacy waiter word 7 (offset `0x38`) as the
fake lock, logs all ten waiter words and their pointer ranges, exercises the
futex/pselect result route, and stops before the `sched_setattr` syscall that
would enter this priority-chain path.

## Fourth hardware result

The first pre-trigger geometry run captured in
`RootMyGalaxy-20260821-042721-failed.log.txt` used exactly one attempt and did
not reboot or panic. Page preparation completed in 4.957 seconds with collision
confirmations 3, reclaim 16/16, and object index 11. Its derived addresses
again resolve into verified RAM:

```text
pipe direct map:  ffffffc1b2f80000 -> physical 0x232f80000 (RAM 2)
page direct map:  ffffffc0cb718000 -> physical 0x14b718000 (RAM 1)
fake task:        ffffffc0cb718180 -> physical 0x14b718180 (RAM 1)
fake lock:        ffffffc0cb71c380 -> physical 0x14b71c380 (RAM 1)
```

All pointer-range checks passed. Waiter words 0 through 7 and word 9 matched,
including word 7 at offset `0x38` containing the valid fake-lock address. The
diagnostic rejected only word 8: actual `0x82`, expected `0`.

That mismatch was in the checker, not the generated fd-set. The legacy stale
pselect waiter deliberately uses `FAKE_WAITER_PRIO=130` (`0x82`), while the
separate waiter stored in the reclaimed page uses
`SLIDE_FAKE_WAITER_PRIO=0`. `prepare_slide_pselect_fdsets()` correctly emitted
the former, but the geometry checker compared it with the latter. Because the
fail-closed check rejected the slot, this run did not enter the futex/pselect
route and did not approach `sched_setattr`.

The corrected route diagnostic now compares word 8 with `FAKE_WAITER_PRIO`,
logs both priority values explicitly, and retains the compile-time exclusion
of the `sched_setattr` call. A new payload ID and artifact path prevent the app
from reusing the rejected geometry binary.

## Connected-device identity

```text
model: SM-T870
device: gts7lwifi
product: gts7lwifixx
display build: TP1A.220624.014.T870XXS8DXH1
fingerprint: samsung/gts7lwifixx/gts7lwifi:13/TP1A.220624.014/T870XXS8DXH1:user/release-keys
Android SDK: 33
security patch: 2024-08-01
ABI: aarch64
kernel release: 4.19.113-27114284
kernel build: #1 SMP PREEMPT Mon Aug 5 15:36:53 +07 2024
SoC: Qualcomm SM8250 / kona
```

The live kernel banner is:

```text
Linux version 4.19.113-27114284 (dpi@VPHMRB636) (clang version 10.0.6 for Android NDK) #1 SMP PREEMPT Mon Aug 5 15:36:53 +07 2024
```

Verified Boot was green, the bootloader state was locked, SELinux was
Enforcing, and the connected device reported a production `user` build with
`ro.debuggable=0` and `ro.secure=1`.

## Captured evidence

The capture directory is outside the cloned repository:

```text
../device-logs/SM-T870_T870XXS8DXH1_20260820_153520/
```

Important retained objects are:

| Object | Size | SHA-256 |
| --- | ---: | --- |
| `logcat-all.txt` | 42,639,276 | `2C2DB2C65AB70631C71DA1C490F9F1E09C42F746787DC911F9DF3B4EFE61D1C8` |
| `bugreport.zip` | 19,055,478 | `FCA58A811A92B458A2BD1B0EA218053B174391260FA4DBAF3BB7C747AEF3007D` |
| stock `boot.img` | 71,303,168 | `F555FF2BB68180A49D4F5617946617F2728A22E536B863A4F2FD31933A4F7D72` |
| raw ARM64 kernel | 55,459,852 | `8E2626AC81C0C857617BB870C92A80EF98987174CEF02D6EDB68E4708E4D603E` |
| recovered `vmlinux.elf` | 62,745,110 | `AB58BD429E60EE3B52795BDCED1CB0A6960C28BA3BC06722D44E82DD95620ADC` |

Direct shell reads of `dmesg`, `/proc/last_kmsg`, pstore, and kallsyms are
blocked on the stock production build. The Samsung bugreport nevertheless
contained recovery copies of `last_kernel`, `last_kmsg`, and `last_log`, which
were extracted under `bugreport-extracted/FS/cache/recovery/`. The recovered
boot log reports `10159276K/12231956K` available/total memory, with
`1613928K` reserved and `458752K` CMA-reserved.

## Stock kernel image and configuration

The boot image uses Android boot header version 2 and a 4096-byte page. Its raw
ARM64 Image header reports:

```text
text_offset: 0x00080000
image_size:  0x047d0000
flags:       0x0000000a
```

`vmlinux-to-elf` recovered 152,597 symbols and applied 145,306 relocations.
The recovered ELF uses `_text = 0xffffff8008080000`; every offset below is
relative to that exact address.

IKCONFIG confirms the relevant stock options:

```text
CONFIG_ARM64_VA_BITS=39
CONFIG_ARM64_4K_PAGES=y
CONFIG_FUTEX=y
CONFIG_FUTEX_PI=y
CONFIG_KALLSYMS=y
CONFIG_KALLSYMS_ALL=y
CONFIG_RELOCATABLE=y
CONFIG_RANDOMIZE_BASE=y
CONFIG_ASHMEM=y
CONFIG_ION=y
CONFIG_CONFIGFS_FS=y
CONFIG_MODULES=y
CONFIG_MODVERSIONS=y
CONFIG_MODULE_SIG_FORCE=y
CONFIG_RSEQ=y
```

Samsung hardening includes DEFEX, PROCA, RKP/UH, KDP, CFP JOPP/ROPP, and LKM
blocking. These features must be treated as part of the target, not as optional
follow-up hardening.

The live allocator exposes a `mm_struct` object size of `0x400` and slab order
3. Only normal and reclaim `kmalloc` families are present; there is no separate
cgroup cache family. `/dev/ashmem`, `/dev/ion`, configfs, and tracefs are
present; `/dev/dma_heap` is absent.

## Recovered symbol offsets

| Use | Exact symbol/slot | Offset from `_text` |
| --- | --- | ---: |
| UMH callback | `call_usermodehelper_exec_work` | `0x000574b4` |
| trace caller | instruction after `worker_thread -> schedule` | `0x0005e7c4` |
| llseek | `noop_llseek` | `0x0024ac7c` |
| splice candidate | `generic_file_splice_read` | `0x0028e26c` |
| splice candidate | `default_file_splice_read` | `0x002900f4` |
| configfs read | `configfs_read_file` | `0x0030414c` |
| configfs write | `configfs_write_bin_file` | `0x0030464c` |
| ashmem ioctl | `ashmem_ioctl` | `0x011bc814` |
| ashmem compat ioctl | `compat_ashmem_ioctl` | `0x011bd0a4` |
| ashmem mmap | `ashmem_mmap` | `0x011bd0fc` |
| ashmem open | `ashmem_open` | `0x011bd27c` |
| ashmem release | `ashmem_release` | `0x011bd304` |
| ashmem fdinfo | `ashmem_show_fdinfo` | `0x011bd404` |
| pipe ops | `anon_pipe_buf_ops` | `0x01da1e00` |
| ashmem fops | `ashmem_fops` | `0x01f1a358` |
| logger name | first pointer target of `nfulnl_logger` | `0x02902abe` |
| allocator table | `kmalloc_caches` | `0x029146b8` |
| SELinux state | global `selinux_enforcing` | `0x0292a200` |
| workqueue | `system_unbound_wq` | `0x0319e708` |
| logger object | `nfulnl_logger` | `0x031a3e70` |
| init task | `init_task` | `0x031ad980` |
| boot-id data pointer slot | `random_table[]` entry named `boot_id` | `0x03249f98` |
| ashmem misc fops slot | `ashmem_misc + 0x10` | `0x0330d1e8` |
| root task group | `root_task_group` | `0x034e8c00` |
| boot-id storage | `sysctl_bootid` | `0x03684e6c` |

Relocations verify that `ashmem_misc + 0x10` points to `ashmem_fops`, the
`random_table[]` slot points to `sysctl_bootid`, and the first qword of
`nfulnl_logger` points to the recovered logger-name string.

The live `sched_blocked_reason` trace event ID is 72. The post-`schedule`
instruction in `worker_thread` is `0xffffff80080de7c4`, yielding the trace
caller offset shown above.

## Confirmed 4.19 layouts

The stock binary, rather than the unrelated local 4.19.325 debug build, gives:

```text
rt_mutex_waiter.pi_tree_entry: 0x18
rt_mutex_waiter.task:          0x30
rt_mutex_waiter.lock:          0x38
rt_mutex_waiter.prio:          0x40
rt_mutex_waiter.deadline:      0x48
sizeof(rt_mutex_waiter):       0x50

task_struct.usage:             0x068
task_struct.prio:              0x0ac
task_struct.normal_prio:       0x0b4
task_struct.sched_task_group:  0x3d8
task_struct.pi_lock:           0x8c8
task_struct.pi_waiters:        0x8e0
task_struct.pi_top_task:       0x8f0
task_struct.pi_blocked_on:     0x8f8
```

The stock Android 4.19 `struct file_operations` is `0x120` bytes. Its relevant
members are `unlocked_ioctl=0x48`, `compat_ioctl=0x50`, `mmap=0x58`,
`open=0x68`, `release=0x78`, `splice_read=0xc0`, and `show_fdinfo=0xd8`.
These offsets must not be copied from the repository's 5.10/6.x targets.

The 4.19 configfs callbacks use the legacy `.read` and `.write` slots at
`0x10` and `0x18`, not the iterator slots. Stock disassembly gives this exact
`configfs_buffer` layout:

```text
configfs_buffer.page:             0x10
configfs_buffer.mutex:            0x20
configfs_buffer.needs_read_fill:  0x60
configfs_buffer.read_in_progress: 0x64
configfs_buffer.bin_buffer:       0x68
configfs_buffer.bin_buffer_size:  0x70
configfs_buffer.cb_max_size:      0x74
configfs_buffer.item:             0x78
configfs_buffer.owner:            0x80
configfs_buffer.attr/bin_attr:    0x88
```

`LEGACY_CONFIGFS_FILE_RW=1` now selects those slots in the shared fake-fops
builder. The matching splice function is `default_file_splice_read`, because
that path invokes the legacy `.read` callback.

The remaining workqueue, page, and pipe layouts were checked against the
matching stock disassembly:

```text
workqueue_struct.dfl_pwq:      0xa0
pool_workqueue.pool:           0x00
pool_workqueue.wq:             0x08
pool_workqueue.work_color:     0x10
pool_workqueue.refcnt:         0x18
pool_workqueue.nr_in_flight:   0x1c
pool_workqueue.nr_active:      0x58
pool_workqueue.max_active:     0x5c
worker_pool.worklist:          0x20
worker_pool.nr_idle:           0x34
work_struct.data/entry/func:   0x00 / 0x08 / 0x18

sizeof(struct page):           0x40
page.compound_head:            0x08
page.slab_cache:               0x18
page.page_type:                0x30

sizeof(struct pipe_buffer):    0x28
pipe_buffer.page:              0x00
pipe_buffer.offset/len:        0x08 / 0x0c
pipe_buffer.ops:               0x10
pipe_buffer.flags:             0x18
pipe_buffer.private:           0x20
```

This kernel predates `PIPE_BUF_FLAG_CAN_MERGE`. Its `pipe_write` reads the
first integer in `pipe_buf_operations` as `can_merge`; the real buffer's
`flags` word is zero. The target value for the repository's compatibility
macro is consequently `PIPE_BUF_FLAG_CAN_MERGE=0`, while the recovered
`anon_pipe_buf_ops` supplies `can_merge=1`.

## pselect route analysis and result-fdset prototype

Let `E` be the kernel stack pointer at system-call wrapper entry for the reused
thread. The stock disassembly gives:

```text
__arm64_sys_futex frame: 0x70
do_futex frame:          0x1e0
WAIT_REQUEUE_PI waiter:  do_futex sp + 0xc0 = E - 0x190

__arm64_sys_pselect6 frame: 0xa0
core_sys_select frame:      0x1c0
stack fd-set base:          core sp + 0x50 = E - 0x210
```

The stale waiter therefore begins 16 qwords after the fd-set base. On this
kernel the stack path is used only when the rounded fd count is at most 320,
which gives five qwords per set and only 15 user-controlled qwords across the
read/write/exception input sets. Qword 16 is in the kernel result-set area,
not user-controlled. Increasing `nfds` moves all six fd sets to a `kvmalloc`
buffer, removing the stack overlap entirely.

The direct input route still has no safe `SLIDE_PSELECT_WORD_SHIFT` on this
exact build and must not be selected.

The overlap is nevertheless usable through the result fd-sets. With five
qwords per set, the legacy waiter maps as follows:

```text
waiter[0..3] -> res_in[1..4]
waiter[4..8] -> res_out[0..4]
waiter[9]    -> res_ex[0] (zero deadline)
```

The macro-gated `SLIDE_PSELECT_RESULT_ROUTE` branch duplicates `/dev/null`
onto every selected read/write descriptor and uses logical input word shift 1.
`pselect6` materializes the same masks in its result sets, the userspace return
masks are compared byte-for-byte with the requested masks, and only then is
the scheduler consumer released. The pselect thread spins without another
system call until the consumer has issued `sched_setattr` against it.

The new branch is compile-time restricted to the legacy `0x50`-byte waiter and
`SLIDE_PSELECT_WORD_SHIFT=1`. Both the result route and the unchanged default
route pass host ARM64-independent C syntax checks with warnings promoted to
errors. This is still a static prototype: no target header is emitted and no
device execution is authorized until the diagnostic-only route is reviewed.

## skb reclaim geometry

The exact `unix_stream_sendmsg` disassembly caps a message at `0x8e80` and
computes paged data from `size - 0xe80`. For that maximum message the linear
head is `0xe80` and the order-3 fragment is `0x8000` bytes. The stock
`skb_copy_datagram_from_iter` first consumes the linear head and then copies
the fragment without adding source alignment, so the fragment begins at user
iov offset `0xe80`. The offline target values are therefore:

```text
SKB_DATA_DELTA: -0xe80
SKB_FRAG_BIAS:   0
```

This conclusion comes from the matching stock binary and its 4.19 copy path;
it does not reuse the `-0x1000` correction required by later kernels.

## Physical map and exact-build P0 table

The recovered Samsung `last_kernel` contains the ABL handoff records:

```text
Memory Base Address: 0x80000000
RAM 0: base 0x080000000, length 0x039900000
RAM 1: base 0x0c0000000, length 0x140000000
RAM 2: base 0x200000000, length 0x180000000
```

The three lengths total the logged DDR size `0x2f9900000`. The exact ARM64
Image header has `text_offset=0x80000`; under the arm64 boot protocol this
places the Image at `0x80080000`. Together with the stock 4.19 VA_BITS=39
layout formulas, the offline constants are:

```text
KIMAGE_TEXT_BASE:    0xffffff8008080000
P0_PAGE_OFFSET:      0xffffffc000000000
P0_PHYS_OFFSET:      0x0000000080000000
P0_KERNEL_PHYS_LOAD: 0x0000000080080000
DIRECT_MAP_BASE:     0xffffffc000000000
DIRECT_MAP_RAM_END:  0xffffffc300000000
VMEMMAP_START:       0xffffffbf00000000
```

The Android boot-header `kernel_addr=0x8000` is a legacy field and is not used
as the arm64 Image placement offset; the embedded Image header and ABL memory
base form the relevant handoff pair.

`src/targets/gts7lwifi-T870XXS8DXH1/p0_fingerprint.h` was generated from the
exact raw Image at probe offset `0x1f0000`. It contains all 32 candidates from
`0x000000` through `0x1f0000`; the generator independently reopened the Image
and verified all 256 sampled qwords.

## Diagnostic-only target

`src/targets/gts7lwifi-T870XXS8DXH1/target.h` now contains the exact offline
constants, but it is deliberately not a production/root profile. The current
artifact is a pre-trigger geometry diagnostic with layered stops:

```text
non-APP_PAYLOAD build: preprocessing error
APP_PAYLOAD build: APP_SLIDE_DIAGNOSTIC_ONLY=1
fresh-session behavior: APP_REQUIRE_FRESH_P0_SESSION=1
current hardware stage: futex/pselect result fd-set validation
geometry: ten legacy waiter words plus pointer ranges logged
hard stop: before sched_setattr(); RT priority-chain trigger not entered
post-slide behavior: return before fops/configfs/UMH
```

The complete app source set passes host syntax validation with warnings
promoted to errors except for the repository's pre-existing transposed
`calloc` diagnostic. A non-app compile was separately verified to fail on the
diagnostic-only `#error`.

The diagnostic artifact was built with Android NDK r29 revision
`29.0.14206865` for Android API 33:

```sh
make TARGET=gts7lwifi-T870XXS8DXH1 API=33 \
  ANDROID_NDK_HOME=/path/to/android-ndk-r29 release
```

```text
artifact: artifacts/gts7lwifi-T870XXS8DXH1/cve-2026-43499-app-pretrigger-route.so
format:   ELF64 little-endian AArch64 shared object, Android 33, stripped
size:     104128 bytes
SHA-256:  cb06149c3a6d5346d39a892b4516fc9802d1b918e09f55c5a4cca62a1b24ff01
```

The diagnostic target adds hidden symbol visibility before section garbage
collection. Post-link inspection retains the `slide diagnostic-only stop`
marker but finds no root helper, `call_usermodehelper`, `misc_fops`, configfs,
or root/fops/configfs exported-symbol markers. The fixed-size artifact is an
exact byte-for-byte copy of the verified release output.

## Remaining gates before the RT-mutex trigger is re-enabled

1. Run the pre-trigger diagnostic and require all six values in
   `p0 pretrigger range` to equal one.
2. Require `p0 pretrigger diagnostic stop` and confirm that it says
   `sched_setattr not entered` without a reboot.
3. Reconcile the observed result-fdset route with the exact 4.19 stack layout;
   do not infer kernel-stack alignment from the user-space word check alone.
4. Do not enable `sched_setattr`, create a root/UMH profile, or replace the
   KernelSU stub until the priority-chain geometry is validated.
