# SM-T870 / T870XXS8DXH1 hardware work log

This is the active evidence and decision log for the SM-T870 port. The stable
firmware profile remains documented in `SM-T870-T870XXS8DXH1.md`; this file is
updated while hardware validation is in progress.

## Goal

Run the repository's existing CVE-2026-43499 app-domain exploit on the exact
stock `T870XXS8DXH1` kernel, obtain the existing physical read/write and root
path, late-load the exact KernelSU module, and then collect dmesg, logcat, and
pstore before installing a custom kernel or ROM.

## Non-negotiable porting rules

1. Follow `docs/PORTING.md` and the exploit source referenced by the repository.
2. Do not invent a replacement payload or a new exploit route.
3. Derive every kernel-dependent change from the exact stock 4.19.113 source,
   recovered target image, disassembly, configuration, or hardware evidence.
4. Do not copy 5.10, 6.1, or 6.6 target tuning into T870 without proving that
   the same kernel path and layout apply to Samsung 4.19.
5. Do not change KernelSnitch, reclaim, pselect, or rt_mutex parameters merely
   to try combinations. Instrument an existing boundary first, then change the
   smallest version-dependent behavior supported by evidence.
6. Keep `support/targets-v3.json` artifact URLs repository-relative.
7. Keep the release exploit exactly 104128 bytes and verify the published raw
   artifact hash after every push.
8. KernelSU work begins only after the exploit reaches the root-helper path;
   a failure before that point is not a KernelSU failure.

## Exact target

```text
model: SM-T870
device: gts7lwifi
display build: TP1A.220624.014.T870XXS8DXH1
kernel: 4.19.113-27114284 #1 SMP PREEMPT Mon Aug 5 15:36:53 +07 2024
stock source: common/SM-T870_EUR_13_Opensource
defconfig: arch/arm64/configs/vendor/gts7lwifi_eur_open_defconfig
```

## Current published checkpoint build

```text
label: gts7lwifi-T870XXS8DXH1-app-ks-collision-checkpoint
artifact SHA-256: 150b3795dfa4da86485d033b9ac02ffe509deb9032e8a4917e0190328fdedb2d
```

Only `APP_KERNEL_PAGE_DIAGNOSTIC_CHECKPOINTS` is enabled. KernelSnitch verbose
mode, diagnostic stop, exploit parameters, and pselect timing are unchanged
from the preceding production build.

## Hardware observations

### 2026-08-21 12:03 and 12:53

Production runs stopped after the `KernelSnitch profile` line and rebooted
with Samsung `KPON`. The retained pstore data was truncated before the
initiating watchdog stack, so those runs did not locate the failing stage.

### Page-prepare stop diagnostic

The existing page-prepare diagnostic completed seven attempts through
collision search, mm-struct brute force, payload construction, skb reclaim,
and its deliberate stop. It used verbose logging and therefore had different
timing. It proves that the shared path can complete, but does not prove that
the production-timed path is safe.

### 2026-08-21 13:38 screen evidence

The app screen is the authoritative surviving log because the persisted
failure file was not flushed after the crash. It showed:

```text
stage=collision-search-return mode=1
stage=bruteforce-enter mode=1
stage=bruteforce-return mode=1 leaked=ffffffc05d61b000
mm leaked=ffffffc05d61b000 base=ffffffc05d618000 object_index=12
stage=payload-ready mode=1 base=ffffffc05d618000
fake_task=ffffffc05d618180 fake_lock=ffffffc05d61c380
mm late cpu-partial drain triggers=32
sk_buff reclaim sends=16/16 mode=1 stop_errno=0
```

No `stage=complete`, page-search result, pselect, P0 gate, root-helper, or
KernelSU line appeared. The device subsequently created
`SYSTEM_LAST_KMSG_10_20260821_133933_KP`; its 29277-byte kernel tail is again
truncated before the initiating stuck-CPU stack.

Correction: the 194-byte persisted failure file ending at `KernelSU download`
was not evidence that the exploit had never executed. The on-screen log above
shows that the payload did execute; the file simply did not retain later output.

## Current boundary

The confirmed failure interval is after all 16 reclaim sends and before
`prepare_kernel_page()` returns. In current shared source that interval is:

1. optional `log_mm_slabinfo("after-exact-drain-reclaim")`;
2. `kernelsnitch_cleanup(ks)` memory unmaps;
3. remaining `prepare_ctx` memfd closes and child `SIGKILL + waitpid` cleanup;
4. `stage=complete` log.

The optional slab logger is a no-op unless `SLUB_DIAG` is present. The broad
interval does not yet justify changing reclaim counts or cleanup ordering.

## Next evidence steps

1. Compare this cleanup sequence with the repository-referenced upstream
   exploit instead of designing a new sequence.
2. Audit the exact Samsung 4.19 source for process exit, `mmput`, futex cleanup,
   and task reaping behavior touched by the existing child cleanup.
3. Determine whether an existing version/target switch already covers that
   kernel difference.
4. If source evidence requires a 4.19-specific adjustment, implement it behind
   a named target flag and preserve the existing exploit route.
5. Record the source evidence, build hash, commit, and next hardware result in
   this file before moving to the next boundary.

## Source audit: post-reclaim cleanup

The repository README references NebuSec/CyberMeowfia as the exploit source.
The audited upstream checkout was commit
`e8c777c29473455c4f4032775775ae3018d5f82a`. Its `prepare_kernel_page()` uses
the same post-reclaim order as this repository: `kernelsnitch_cleanup()` first,
then close every retained prepare memfd and kill/reap every prepare child.
There is no upstream alternate 4.19 cleanup sequence to copy.

The exact Samsung 4.19.113 source confirms:

- `fs/proc/base.c::proc_mem_open()` obtains an mm reference, converts it to an
  `mm_count` pin with `mmgrab()` plus `mmput()`, and `mem_release()` drops that
  pin with `mmdrop()`;
- child exit enters `kernel/fork.c::mm_release()`, including robust-futex and
  PI-state cleanup when present, before `kernel/exit.c::exit_mm()` calls
  `mmput()`;
- the final `mmdrop()` reaches `__mmdrop()` and returns the exact `mm_struct`
  object through `kmem_cache_free(mm_cachep, mm)`.

This proves that the existing proc-mem lifetime mechanism is present on the
target. It does not identify whether the current watchdog occurs during the
64-GiB KernelSnitch mapping unmap or while reaping the remaining prepare
children.

## Decision: cleanup checkpoint only

The next payload does not alter cleanup order or any exploit parameter. It
enables the cleanup messages already present in `src/util.c` whenever
`APP_KERNEL_PAGE_DIAGNOSTIC_CHECKPOINTS` is selected. The expected new boundary
messages are:

```text
kernel page cleanup stage=kernelsnitch begin
kernel page cleanup stage=kernelsnitch done
kernel page cleanup stage=prepare-children begin
kernel page cleanup stage=prepare-children progress=256/1024
kernel page cleanup stage=prepare-children done
```

Only after hardware identifies which original cleanup stage fails will a
4.19-specific behavioral change be considered.

### 2026-08-21 13:57 cleanup-checkpoint run

The app loaded the payload labelled
`gts7lwifi-T870XXS8DXH1-app-in-kernel-cleanup-checkpoint` and printed the
startup context, build configuration, and P0 profile. Its final surviving
line was:

```text
p0 profile pid=16650 phys_offset=0000000080000000
kernel_phys_load=0000000080080000 delta=0000000000080000
slide_logger=ffffffc002982abe bootid_data=ffffffc0032c9f98
init_task=ffffffc00322d980 root_tg=ffffffc003568c00
sysctl_bootid=ffffffc003704e6c
```

Neither `p0 pipe oracle prepared` nor
`kernel page diagnostic stage=kernel-page-prepare-enter` appeared. The device
then rebooted with boot ID `9d56e935-807d-4af1-af32-fc258e2cbecd` and created
`SYSTEM_LAST_KMSG_11_20260821_135748_KP`. That 12615-byte DropBox entry contains
bootloader output but no useful initiating kernel stack.

This run therefore failed inside the existing `prepare_p0_pipe_oracle()` ->
`prepare_pipe_buffer_page()` -> `prepare_pipe_buffer_page_child()` bootstrap,
before the cleanup checkpoint added for the later `prepare_kernel_page()`
path. It does not supersede the earlier screen evidence that reached all 16
reclaim sends. The existing pipe-page child contains the repository's normal
KernelSnitch collision, bruteforce, skb reclaim, pipe-cache shaping, and pipe
reallocation sequence, but it currently has no stage boundary messages.

Decision: do not change exploit order, counts, timing, or target constants
from this earlier-stage failure. The next diagnostic build exposes only the
existing pipe-page child stage boundaries under the same diagnostic build
flag, then uses the resulting last line to select the exact source audit.

### Published P0 pipe checkpoint build

The existing `prepare_pipe_buffer_page_child()` sequence is unchanged. The
diagnostic flag now prints boundaries after its original setup, topology,
collision search, bruteforce, KernelSnitch cleanup, pipe-cache shaping, pipe
allocation, and context cleanup calls. In particular,
`bruteforce-return` and `kernelsnitch-cleanup-return` are separate so the
same cleanup interval observed later in `prepare_kernel_page()` can be tested
without changing either cleanup implementation.

```text
label: gts7lwifi-T870XXS8DXH1-app-p0-pipe-checkpoint
artifact size: 104128
artifact SHA-256: 7d2a3c451be26de3ea0f83e11c3ed9b7012d604e3b56e814c2acf3e3ea187427
```

No KernelSnitch count, mm spray/reclaim count, scheduler yield, pipe count,
cleanup order, target address, or pselect timing was changed. The support feed
still selects the same payload ID and repository-relative artifact URL; its
size remains 104128.

### 2026-08-21 14:15 P0 pipe checkpoint run

The device loaded
`gts7lwifi-T870XXS8DXH1-app-p0-pipe-checkpoint` and reached:

```text
p0 pipe page diagnostic stage=begin objects_per_slab=32
p0 pipe page diagnostic stage=prep-spray-ready prep=1024 spray=192
KernelSnitch profile mode=1 fast=0 appended=2048 repeat=64 average=8
p0 pipe page diagnostic stage=kernelsnitch-ready
p0 pipe page diagnostic stage=topology-ready pre=31 post=32 leak_pid=17088
```

It did not print `collision-search-return`. The subsequent boot ID was
`e8893555-92a7-4e22-bac3-ecf5cddea6ab`, and DropBox created
`SYSTEM_LAST_KMSG_12_20260821_141508_KP`. The retained 12564-byte entry again
contains no initiating kernel stack.

The parent is blocked in `waitpid(leak_child)` during this interval while the
child runs the repository's existing `kernelsnitch_find_collisions()`.
Therefore the active failure interval is now limited to that function's
baseline futex measurement, waiter pileup, collision scan, or waiter teardown.

The device reports eight online and eight possible CPUs. Both the exact
Samsung 4.19 `futex_init()` and KernelSnitch's userspace hash model therefore
select 2048 buckets. The exact 4.19 `futex_wake()` takes the bucket spinlock
and traverses its waiter chain when `hb_waiters_pending()` is true.
KernelSnitch deliberately queues 2048 waiters into one bucket and measures
`FUTEX_WAKE_PRIVATE` traversal with 64 samples. This explains which stock
kernel path is under load, but does not yet prove that a different waiter or
sample count is required: the earlier page-prepare stop diagnostic completed
the same profile seven times.

Decision: keep all collision and measurement parameters unchanged. Add phase
checkpoints around the existing baseline, pileup, scan, and decrease calls,
without logging inside any timed measurement. The next surviving last line
will identify the exact existing phase to audit before considering a
4.19-specific behavioral change.

### Published KernelSnitch collision phase build

The new checkpoints are outside the timed `__measure()` loop:

```text
baseline-enter
baseline-return
pileup-enter
pileup-return
scan-enter
scan-return
decrease-enter
decrease-return
```

The build label is `gts7lwifi-T870XXS8DXH1-app-ks-collision-checkpoint`.
The 104128-byte artifact SHA-256 is
`150b3795dfa4da86485d033b9ac02ffe509deb9032e8a4917e0190328fdedb2d`.

### 2026-08-21 14:34 incomplete history recovery and correction

The app did preserve the latest run, but not as a finalized exported failure
log. Its atomic history writer left
`files/install-history/dbb7cc50-863e-4a4f-80ff-34be75c1391d.json.new` with
`result=Running` when the device rebooted. Treat `.json.new` as authoritative
crash evidence instead of concluding that no log exists.

This recovered log invalidates the preceding KernelSnitch failure hypothesis.
The run completed all of these existing stages:

- P0 pipe collision search, bruteforce, KernelSnitch cleanup, pipe allocation,
  and context cleanup;
- P0 pipe oracle preparation;
- the second kernel-page collision search and bruteforce;
- all 16 skb reclaim sends;
- KernelSnitch cleanup;
- prepare-child cleanup through 1024/1024;
- kernel-page completion and page-search selection.

The exact final lines were:

```text
p0 fresh page attempt=1/1 base=ffffffc0592f0000
slide child context route=pselect pid=22033 uid=10331 euid=10331
slide wait_requeue_pi ret=-1 errno=110
```

No `slide cmp_requeue_pi` result followed. The device rebooted around 14:34
and created `SYSTEM_LAST_KMSG_13_20260821_143501_KP`; the retained 12610-byte
entry contains only bootloader output and no initiating futex/rt_mutex stack.

The waiter's `ETIMEDOUT` is accepted by the current code. After that, the
waiter spins until the coordinating thread's existing
`FUTEX_CMP_REQUEUE_PI` call returns `EDEADLK`. The missing return log limits
the active kernel failure interval to that PI-requeue/deadlock operation.

The referenced upstream uses the same wait-requeue/PI-chain exploit route, but
does not contain the repository's later EDEADLK validation and polling around
the coordinating `FUTEX_CMP_REQUEUE_PI` call. Before any new payload build,
audit that repository change against Samsung 4.19's
`futex_requeue()`, `futex_lock_pi_atomic()`, proxy-lock, and rt_mutex
deadlock handling. Do not make further KernelSnitch, reclaim, or cleanup
changes based on this run.

### 2026-08-21 PI-requeue ordering audit and T870 correction

The app-private history was searched again using the unquoted tokens
`wait_requeue_pi` and `cmp_requeue_pi`. Across all retained text histories,
the only PI-route return record is:

```text
slide wait_requeue_pi ret=-1 errno=110
```

There is no retained textual `slide cmp_requeue_pi` return. This confirms that
the latest missing line is not just an export truncation and that no older
retained run proves the repository's EDEADLK gate completes on this device.

The exact Samsung 4.19 source in
`common/SM-T870_EUR_13_Opensource/kernel/futex.c` was compared with both the
current app payload and the referenced CyberMeowfia exploit. In
`futex_requeue()`, PI requeue enters `futex_proxy_trylock_atomic()` and then
`rt_mutex_start_proxy_lock()` before the syscall can return. The waiter-side
`futex_wait_requeue_pi()` performs proxy-lock cleanup and owner fixup before
returning. Therefore the current app-only ordering can make userspace wait for
the coordinator's syscall result while that kernel path still depends on PI
chain progress.

The repository's app payload had added two conditions that are absent from
the referenced exploit: the waiter kept `slide_f_pi_chain` locked until the
coordinator observed EDEADLK, and the coordinator polled until that exact
result. The referenced exploit instead unlocks the chain immediately after
`FUTEX_WAIT_REQUEUE_PI` returns and treats `FUTEX_CMP_REQUEUE_PI` as the
trigger rather than a required EDEADLK oracle. The repository's older
`src/main.c` route likewise does not gate progress on the requeue return.

Add `SLIDE_UPSTREAM_PI_REQUEUE_ORDER` only to the T870 target. With this flag,
the existing 50 ms wait result is still logged and must still be ETIMEDOUT,
but the waiter then unlocks the chain immediately. The coordinator performs
one upstream-style `FUTEX_CMP_REQUEUE_PI`, logs the result for diagnosis, and
does not require EDEADLK. All page preparation, KernelSnitch, reclaim, waiter
layout, pselect word shift, and physical-write code remains unchanged. Other
targets retain the existing guarded behavior.

The release build completed with NDK r29 and retained the required fixed size.
The published artifact candidate is:

```text
label:  gts7lwifi-T870XXS8DXH1-app-4.19-pi-order
size:   104128
sha256: 83516e34799fe3c6fac4e35b2084bc7f1461056fb547402975e70384762556ef
```

`support/targets-v3.json` parses successfully, still points to the repository-
relative artifact path, and contains no BuSung-dev absolute raw URL.

### 2026-08-21 15:05 first PI-order hardware result

The device downloaded and executed the exact published artifact. Its private
copy retained size 104128 and SHA-256
`83516e34799fe3c6fac4e35b2084bc7f1461056fb547402975e70384762556ef`.
The incomplete history
`4a2f5274-f6d5-49ef-9796-d26db3dd7d20.json.new` identifies the new build label
and ends with:

```text
slide cmp_requeue_pi ret=-1 errno=35 polls=1
slide wait_requeue_pi ret=-1 errno=110
```

This is the first retained T870 run in which the coordinating PI-requeue call
returned. It proves the prior missing-return interval was removed and also
proves that Samsung 4.19 does return EDEADLK for this constructed chain. The
device then rebooted and created
`SYSTEM_LAST_KMSG_14_20260821_150633_KP`. The retained kernel text identifies
`cve43499-run` as the initiating process but does not preserve a clean causal
futex/rt_mutex stack.

The new active interval begins after the wait return and before the existing
pselect-return log. The T870 upstream-order branch currently skips the
repository's `slide_owner_acquired` synchronization. Earlier T870 pretrigger
diagnostics that reached and returned from pselect used that synchronization.
Keep the immediate chain unlock and single upstream-style requeue call, but
restore the owner-acquired wait only for T870 before entering pselect. Add
checkpoints after chain unlock, after owner acquisition, and before pselect;
do not change page preparation, reclaim, layout, or pselect payload data.

The owner-sync release candidate built successfully with NDK r29:

```text
label:  gts7lwifi-T870XXS8DXH1-app-4.19-pi-owner-sync
size:   104128
sha256: 640e573857d4145aaa672e187609e09c9b07291a325e832e76426e75cdd2c914
```

### 2026-08-21 15:10 owner-sync hardware result and pselect correction

The device downloaded the exact 104128-byte owner-sync artifact with SHA-256
`640e573857d4145aaa672e187609e09c9b07291a325e832e76426e75cdd2c914`.
The incomplete private history
`f2ae8cb1-959c-48e9-bebe-dc34300ce4f8.json.new` ends with:

```text
slide cmp_requeue_pi ret=-1 errno=35 polls=1
slide wait_requeue_pi ret=-1 errno=110
slide chain unlock complete
slide owner chain acquired
slide pselect enter
```

The device then rebooted before a pselect-return line was preserved. This
proves that the T870-specific PI requeue, chain unlock, and owner-acquired
synchronization all completed. Freeze that sequence; the active failure is the
pselect/scheduler trigger that follows it.

The exact stock ELF was re-audited rather than inferring the failure from the
missing log line. Let `E` be the stack pointer at syscall-wrapper entry:

```text
__arm64_sys_futex frame:       0x070
do_futex frame:                0x1e0
rt_mutex_waiter:               do_futex sp + 0x0c0 = E - 0x190

__arm64_sys_pselect6 frame:    0x0a0
core_sys_select frame:         0x1c0
stack fd-set base:             core sp + 0x050 = E - 0x210
result input fd-set base:      E - 0x198
stale waiter word zero:        result input qword 1
```

For `nfds=320`, each set is five qwords. The input sets occupy overall qwords
0 through 14; the result sets begin at qword 15 and the stale waiter begins at
qword 16. The exact `core_sys_select()` disassembly also shows three `memset`
calls that zero the result sets before `do_select()`. Consequently the current
in-kernel blocked-pselect route cannot supply the stale waiter: its lock qword
is zero at the scheduler trigger. This matches the earlier panic at
`_raw_spin_trylock+0x1c`, where the exact instruction dereferences the supplied
lock pointer.

The original T870 port record had already derived this same geometry and
explicitly rejected the direct input route. Commit `8295018` later replaced
the existing result route with the incompatible in-kernel route; its claim
that the result area was usable while pselect remained blocked was incorrect.

Historical result-route runs are still useful but not conclusive for the
corrected route. They repeatedly proved byte-matching result fdsets and
`sched_setattr` success, then reported `p0 pipe gate hits=0 changed=0`. All of
those binaries used the older PI ordering. The exact gate target remains the
active `pipe_buffer.page` at the leaked order-3 pipe slab plus object offset
`0x800`; no historical run reported a changed page. Therefore do not label the
old result as a reclaim miss without additional evidence.

Next action: restore the existing `SLIDE_PSELECT_RESULT_ROUTE=1` and null
timeout only for T870, retain the hardware-proven upstream PI order and owner
synchronization, and remove the incompatible blocked-pselect guards. This is
the first build that combines the correct result-set geometry with the
corrected PI sequence; it does not add a new exploit primitive or change page
preparation, reclaim, waiter layout, priority values, or the P0 gate.

The result-route/PI-order release built successfully with NDK r29. A forced
release rebuild of the default `pa3q-S938NKSUACZF1` target also completed. The
support manifest parses, keeps repository-relative URLs, and still records the
fixed exploit size:

```text
label:  gts7lwifi-T870XXS8DXH1-app-4.19-result-route-pi-order
size:   104128
sha256: 1cc3b9245cf57572e177d8911a81fe1037e4f76f65e27da39e4c171ba0501eac
url:    artifacts/gts7lwifi-T870XXS8DXH1/cve-2026-43499-app.so
```
