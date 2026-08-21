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
label: gts7lwifi-T870XXS8DXH1-app-4.19-selftarget-no-pipe-retry8
artifact SHA-256: 35c4708bc0f6251985761f2a4e6895d0c67c111ed7b76b4846bdfb79a602f4e3
```

This target-only candidate restores the referenced upstream
`prepare_kernel_page()` mm-to-skb reclaim order. It retains the result-copy
and self-target diagnostics and fixes the supervisor to one hardware attempt.

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

### 2026-08-21 15:36 result-route/PI-order hardware result

The device downloaded the exact published artifact with SHA-256
`1cc3b9245cf57572e177d8911a81fe1037e4f76f65e27da39e4c171ba0501eac`
and reported the expected build label. The finalized private history
`9724d4ff-4e8e-4ef0-8969-b74224f7a763.json` contains four complete attempts
before the app deadline interrupted page preparation for attempt five. Every
complete attempt reached:

```text
slide cmp_requeue_pi ret=-1 errno=35 polls=1
slide wait_requeue_pi ret=-1 errno=110
slide chain unlock complete
slide owner chain acquired
slide pselect enter
slide pselect returned ... ret>0 ... entered=1 calls=1 sched_ok=1
p0 physical write status=0 ok=1
p0 pipe gate hits=0 changed=0
```

The device did not panic or reboot. This validates the result-set placement
and corrected PI sequence together, but it also rules out the old PI ordering
as the explanation for the previous `hits=0 changed=0` results. The remaining
failure lies between the accepted scheduler trigger and the expected
`pipe_buffer.page` change.

Re-audit of the exact 4.19 `unix_stream_sendmsg()` source found a concrete
payload coverage error. `SKB_SEND_SIZE` is 0x10000, while one skb is capped at
`SKB_MAX_HEAD(0) + UNIX_SKB_FRAGS_SZ = 0x8e80`. With the observed 2 MiB send
buffer, one send is split as follows:

```text
skb 1: size=0x8e80 data_len=0x8000 linear=0x0e80
       fragment begins at overall source offset 0x0e80
skb 2: size=0x7180 data_len=PAGE_ALIGN(0x6300)=0x7000 linear=0x0180
       fragment begins at overall source offset 0x9000
```

The existing generator places both order-3 payload copies as though their
fragment data began at `chunk + 0x0e80`. Its first copy is correct. For the
second skb, the gate marker falls in the linear head and every fake object is
0x180 bytes before its assumed address. This can make a correctly reclaimed
second fragment produce an unchanged gate even though the scheduler trigger
returns success.

Add a target-only `SKB_SECOND_PAYLOAD_BIAS=0x180` and make the existing shared
payload loops honor it only for `chunk == ORDER3_SIZE`. This does not change
the skb allocation, reclaim count, timing, PI route, waiter geometry, or gate;
it makes both fragments generated by the exact 4.19 split contain the same
payload at their actual fragment-relative offsets.

The T870 release and a forced default-target regression release both built
successfully with NDK r29. The fixed-size candidate is:

```text
label:  gts7lwifi-T870XXS8DXH1-app-4.19-second-frag-layout
size:   104128
sha256: 5d278571b715247d65609a5d3fa688d81195845141edc2fc98dd41dfd02e60d1
url:    artifacts/gts7lwifi-T870XXS8DXH1/cve-2026-43499-app.so
```

### 2026-08-21 15:56 second-fragment candidate: invalid hardware run

The device used the exact published artifact with SHA-256
`5d278571b715247d65609a5d3fa688d81195845141edc2fc98dd41dfd02e60d1`
and reported the expected `app-4.19-second-frag-layout` label. The finalized
history is `6ebc3b56-8c5b-4ed1-872c-a1d49cb3fbac.json`.

This run is not evidence for or against the second-fragment correction. It
completed in about 7.6 seconds and stopped during the second KernelSnitch
collision scan, before `prepare_good_kernel_page()` returned and before any
PI, pselect, physical-write, or pipe-gate checkpoint:

```text
[*] kernel page diagnostic stage=topology-ready ...
[*] KernelSnitch collision diagnostic stage=pileup-return
[*] KernelSnitch collision diagnostic stage=scan-enter total=65536 wanted=3
```

Android logcat gives the cause at `15:56:01.436`: ActivityManager killed the
payload supervisor PID 20787, payload PID 20788, and the KernelSnitch child
processes with `Trimming phantom processes`. There was no fatal signal report,
kernel panic, or device reboot. Both
`activity_manager/max_phantom_processes` and
`settings_enable_monitor_phantom_procs` are unset, so Android used its default
phantom-process policy.

Do not change the exploit route, skb layout, PI order, or timeout based on this
run. The next hardware run must first prevent Android's phantom-process trimmer
from killing the intentionally large KernelSnitch process group; then rerun
the same published artifact so the second-fragment correction actually reaches
the existing gate check.

For that rerun, set only the scoped ActivityManager key through adb:

```text
device_config activity_manager/max_phantom_processes = 2147483647
```

The previous value was unset (`null`). Restore the device default after the
porting session with `device_config delete activity_manager
max_phantom_processes`. The broader
`settings_enable_monitor_phantom_procs` switch was not changed.

### 2026-08-21 16:10 valid second-fragment run: 24 gate misses

With the scoped phantom-process limit in place, the same published
`app-4.19-second-frag-layout` artifact completed normally. The finalized
history is `cb935eae-979f-4ffa-8d45-07f02d111fa1.json`; it ran for about 426
seconds and completed all 24 independent attempts. ActivityManager did not
trim the supervisor, payload, or KernelSnitch children during this run.

Every attempt completed both KernelSnitch/page preparations and the complete
hardware-proven trigger sequence:

```text
slide cmp_requeue_pi ret=-1 errno=35 polls=1
slide wait_requeue_pi ret=-1 errno=110
slide chain unlock complete
slide owner chain acquired
slide pselect returned ... sched_ok=1 last_sched_ret=0
p0 physical write status=0 ok=1
p0 pipe gate hits=0 changed=0
```

The second-fragment correction is therefore insufficient to make the gate
change, but its `0x180` bias remains correct. The first skb's page fragment
starts at source `0x0e80`. The second starts at `0x9000`; because fake kernel
pointers use `payload_base = base - 0x0e80`, the second generator base must be
`0x9000 - 0x0e80 = 0x8180`, which is chunk `0x8000 + 0x180`. Do not revert or
retune this bias based on the gate misses.

Freeze the now repeatedly validated PI ordering, owner synchronization,
pselect result route and word shift, scheduler trigger, physical write call,
and skb fragment layout. The repeated unchanged gate shifts the active
question to whether the leaked order-3 skb page is actually reclaimed as a
kmalloc-2k pipe-buffer slab.

### Exact 4.19 SLUB audit and next candidate

The stock `mm/slub.c`, stock config, and exact pipe allocation size give:

```text
CONFIG_SLUB=y
CONFIG_SLUB_CPU_PARTIAL=y
CONFIG_NR_CPUS=8
pipe ring bytes: 32 * sizeof(pipe_buffer=0x28) = 0x500 -> kmalloc-2k
kmalloc-2k slab order: 3
objects per slab: 16
min_partial: ilog2(0x800) / 2 = 5
cpu_partial: 6 (the 1024 <= size < PAGE_SIZE branch)
```

The shared exploit constants used `PIPE_CPU_PARTIAL=2`, which is the stock
rule for cache objects at least one page in size, not for this 2 KiB cache.
Also, `PIPE_SHAPE_ROUNDS` has remained zero since the repository's initial
publication, so the already implemented `shape_pipe_cache()` path has never
run. The unshaped 240-object drain can be satisfied by existing per-CPU/node
partial slabs and does not guarantee that the subsequently freed skb order-3
page is selected for the 240 reclaim objects.

Next candidate: preserve the shared defaults, allow target overrides, and for
T870 only set `PIPE_CPU_PARTIAL=6` and `PIPE_SHAPE_ROUNDS=1`. This activates
the repository's existing cache-shaping sequence once with values derived
from this exact kernel. It does not change the exploit primitive, page leak,
PI route, skb payload, drain/reclaim counts, or gate.

The T870 release and a forced default-target regression release both built
successfully with NDK r29. The fixed-size hardware candidate is:

```text
label:  gts7lwifi-T870XXS8DXH1-app-4.19-pipe-slub-shape
size:   104128
sha256: 4333b30112a9230bed311f04a19556b7e8e0db38a3efc3698463c35209bef822
url:    artifacts/gts7lwifi-T870XXS8DXH1/cve-2026-43499-app.so
```

### 2026-08-21 16:38 pipe-SLUB-shape result: quota failure before PI

The device downloaded the exact published
`app-4.19-pipe-slub-shape` artifact. The finalized private history is
`79b7a236-97d7-4afe-a01c-23b5470fd623.json`. All 24 attempts completed the
existing KernelSnitch collision/bruteforce path, printed
`stage=pipe-cache-shaped`, and allocated all 240 drain pipes. Every attempt
then failed while expanding the reclaim pipes with:

```text
F_SETPIPE_SZ: Operation not permitted
```

No PI requeue, pselect, physical-write, or gate line was reached. This run is
therefore invalid as a gate evaluation and does not show whether the shaped
slab would reclaim the leaked skb page.

The failure matches the exact stock `fs/pipe.c` accounting. The default soft
limit is `PIPE_DEF_BUFFERS * INR_OPEN_CUR = 16 * 1024 = 16384` pages. Before
the reclaim expansion, this candidate holds approximately 14008 charged pipe
pages: 704 pipes at the two-page minimum, plus about 180 shaped and 240 drain
pipes expanded from two to 32 slots. Only about 79 additional reclaim pipes
can then expand before the soft limit rejects the request. Do not interpret
this EPERM as a PI or gate miss.

### Recovered earlier diagnostics: allocator-only hypothesis is insufficient

The device still retains the earlier local diagnostic logs and the repository
still retains their local binaries, although these results were accidentally
omitted from this work log during the reset. They must constrain subsequent
work:

- 192-message skb readback runs consumed the complete 12 MiB socket payload
  and reported `changed=0`.
- Four self-target variants made the rb-tree child target point inside the
  same candidate order-3 skb payload page, avoiding only the later pipe-slab
  placement question.
- Each self-target run obtained byte-exact pselect result fd-sets,
  `sched_ok=1`, and successful physical-write status, but the subsequent skb
  readback still reported `mutation=0`.
- One recorded example used payload base `ffffffc258478000` and target
  `ffffffc25847e180`, which are in the same payload allocation.

Correction: these reads do not prove that an skb fragment reclaimed the freed
`mm_struct` page at `base`. They only prove that the bytes returned by the
reclaim socket were unchanged. If none of the 16 skb fragments landed at
`base`, the fake lock never existed at the leaked address and a zero mutation
is expected. The earlier conclusion that the allocator was excluded was
wrong; PI ordering must not be changed further on this evidence.

### Current trigger boundary from exact Samsung 4.19 source

`kernel/sched/core.c::__sched_setscheduler()` calls
`rt_mutex_adjust_pi(p)` whenever the successful `sched_setattr` uses the PI
path. The exact `kernel/locking/rtmutex.c::rt_mutex_adjust_pi()` returns before
the chain walk only when `task->pi_blocked_on` is NULL or the stale waiter's
priority/deadline equals the task's current effective priority/deadline. If it
continues, `rt_mutex_adjust_prio_chain()` validates the stale waiter and lock,
then calls `rt_mutex_dequeue(lock, waiter)`; that rb erase is the expected
write.

The T870 payload currently has two distinct priority values: generated skb
waiters use target-specific `SLIDE_FAKE_WAITER_PRIO=0`, while the pselect
result-stack encoder still uses shared `FAKE_WAITER_PRIO=130`. Do not change
either value blindly. The next diagnostic must record the waiter thread's
actual scheduler priority around the successful `sched_setattr` and preserve
the already proven PI ordering, owner synchronization, pselect result route,
and self-target readback path. This will distinguish the priority-equality
early return from a lost/overwritten `pi_blocked_on` or later chain-walk
failure before allocator tuning resumes.

### Published scheduler-priority boundary diagnostic

The target-only pipe shaping defines were removed so this run returns to the
last quota-safe pipe preparation path. The shared target-override support is
left in place, but `shape_pipe_cache()` is not enabled for T870 in this
candidate.

`APP_SCHED_PRIORITY_DIAGNOSTIC` reads
`/proc/self/task/<waiter-tid>/stat` from the separate consumer thread directly
before and after the unchanged `sched_setattr`. Exact stock
`fs/proc/array.c` exports field 18 as `task_prio(task)`, and exact stock
`kernel/sched/core.c` defines that value as `task->prio - MAX_RT_PRIO`.
The diagnostic therefore adds 100 and logs the actual effective kernel
priority alongside field 19's nice value and the pselect stack waiter's fixed
priority 130. It does not issue another syscall on the stale-waiter thread and
does not alter the pselect stack words, scheduler request, PI order, owner
synchronization, or gate target.

Interpret the new line as follows:

- `before_ok=1` and `after_ok=1` make both priority readings authoritative.
- `after_prio=130` permits the exact `rt_mutex_adjust_pi()` priority-equality
  early return and requires a priority-specific correction.
- `after_prio!=130` rules out that early return; an unchanged self-target then
  points to a missing/overwritten `pi_blocked_on` or a later chain validation
  exit, not the allocator.

The T870 release and forced default-target regression release both built with
NDK r29. The fixed-size candidate is:

```text
label:  gts7lwifi-T870XXS8DXH1-app-4.19-sched-prio-diagnostic
size:   104128
sha256: 74fc01e8668e3671f66f4c3e9b5a88f4301dcee5fb4fbfd3b4eda966e0d1f74c
url:    artifacts/gts7lwifi-T870XXS8DXH1/cve-2026-43499-app.so
```

### 2026-08-21 18:17 scheduler-priority diagnostic result

The device loaded the exact
`gts7lwifi-T870XXS8DXH1-app-4.19-sched-prio-diagnostic` build. The finalized
history is `a175c360-8a26-4b18-8b81-b4c666b40348.json`. All 24 attempts
completed page preparation, PI requeue ordering, owner synchronization,
pselect result-mask validation, the scheduler call, and the physical-write
call. Every attempt then reported `p0 pipe gate hits=0 changed=0`.

All 24 priority readings were identical:

```text
stack_waiter=130
before_ok=1 before_prio=120 before_nice=0
after_ok=1  after_prio=121  after_nice=1
sched_setattr ret=0 errno=0
```

This rules out the exact Samsung 4.19 `rt_mutex_adjust_pi()`
priority-equality early return: the stale waiter's priority 130 differs from
both the pre-call effective priority 120 and the post-call effective priority
121. Do not change either waiter priority based on this failure.

The earlier self-target readback failures and this result together move the
active boundary away from pipe allocation and priority selection. The stale
waiter must either be unavailable by the time the result-route consumer calls
`sched_setattr`, or the subsequent chain validation/dequeue is not reached.

### Exact result-copy window and next 4.19 synchronization

The repository-referenced upstream exploit at commit
`e8c777c29473455c4f4032775775ae3018d5f82a` triggers while pselect is in the
kernel. That works for targets whose waiter overlaps controllable input
fd-sets. T870's exact legacy waiter instead begins at result qword 1, so its
complete data exists only after `do_select()` has populated the kernel result
sets.

Exact stock source and ELF show this order in `core_sys_select()`:

```text
do_select()
copy res_in  to userspace
copy res_out to userspace
copy res_ex  to userspace
return from core_sys_select()
```

The preceding result qword 0 does not overlap the waiter. The next candidate
uses that otherwise unused qword as a copyout sentinel:

1. preserve fd 63, because it may belong to the existing physical pipe
   oracle;
2. temporarily duplicate an empty pipe read end onto fd 63 and select its bit
   only in the input set;
3. arm the existing consumer before entering pselect;
4. busy-wait on the shared userspace input word;
5. call the unchanged `sched_setattr` immediately when the first result copy
   clears the not-ready sentinel bit;
6. restore fd 63 before the physical pipe oracle is inspected.

At the detection point, all fake waiter words are already populated in the
kernel result arrays, while `core_sys_select()` still has the second and third
result copies to perform. This changes only T870's synchronization boundary;
the CVE primitive, futex order, waiter layout and values, fake lock, physical
target, page preparation, and pipe gate are unchanged.

The new diagnostic line reports whether the consumer observed the sentinel
before the pselect thread executed its first userspace instruction after the
syscall:

```text
slide result-copy trigger sentinel_cleared=1 seen_after_return=0 ...
```

The T870 release and forced default-target regression release both built with
NDK r29. The fixed-size candidate is:

```text
label:  gts7lwifi-T870XXS8DXH1-app-4.19-result-copy-trigger
size:   104128
sha256: f6f9c2b61f33d9d1d7dba3615e553c3089198f8fb7cedf4b3abd2577d3783a53
url:    artifacts/gts7lwifi-T870XXS8DXH1/cve-2026-43499-app.so
```

### 2026-08-21 18:46 result-copy hardware result

The device loaded the exact
`gts7lwifi-T870XXS8DXH1-app-4.19-result-copy-trigger` artifact. The finalized
private history is `166b02f8-747e-40a9-b0bf-a64e0130aa3d.json`. All 24
independent attempts completed page preparation, the single upstream-style PI
requeue, chain-owner synchronization, pselect, `sched_setattr`, and the
physical-write wrapper. Every attempt reported:

```text
slide result-copy trigger sentinel_cleared=1 seen_after_return=0 syscall_returned=1
p0 physical write status=0 ok=1
p0 pipe gate hits=0 changed=0
```

The consumer therefore observed the first `core_sys_select()` result copy and
entered `sched_setattr` before the pselect thread executed its first userspace
instruction after syscall return. The failure rules out the earlier
post-return synchronization as the sole cause, but the pipe target still
leaves its separate kmalloc-2k reclaim placement unresolved.

The exact recovered device ELF also confirms that the target is vulnerable,
not merely the released Samsung source. `remove_waiter()` at
`ffffff8008148d74` reads `SP_EL0`, locks `current->pi_lock` at `+0x8c8`, and
stores zero to `current->pi_blocked_on` at `+0x8f8`; it does not load
`waiter->task` before that store. This is the vulnerable CVE-2026-43499 proxy
rollback behavior. The exact `rt_mutex_adjust_pi()` at `ffffff8008147bac`
loads the stale pointer from the target task at `+0x8f8`, the waiter lock at
`+0x38`, and calls the chain walk when the already measured priorities differ.

### Correction to the earlier self-target interpretation

The retained 06:14 and later self-target binaries did not use
`prepare_p0_diag_gate_payload()`. They changed slot zero in the normal
`prepare_skb_payload()` bank and therefore did use T870's already checked
`rt_mutex` offsets `waiters.root=+0x10`, `leftmost=+0x18`, and `owner=+0x20`.
Those readbacks remain valid evidence that the old, userspace-after-return
trigger did not mutate the reclaimed skb payload. They do not test the new
result-copy window.

Separately, the unused arbitrary-R/W diagnostic helper did hardcode the
generic lock offsets `+0x08/+0x10/+0x18`. It is corrected to use the target
macros so it cannot silently construct the wrong 4.19 lock if used later.

### Combined result-copy/self-target boundary

The next candidate combines the two existing diagnostic boundaries without
changing the CVE setup, scheduler request, pselect stack layout, fake task,
fake lock, or skb allocation:

1. slot zero uses the retained self-target geometry from the earlier hardware
   runs: `parent=direct_to_page(base)` and
   `target=payload_base+0x7000`;
2. the result-copy sentinel invokes the unchanged `sched_setattr` while
   `core_sys_select()` is still copying result fd-sets;
3. after the child exits, the existing reclaim socket is drained and compared
   byte-for-byte with the generated 64 KiB payload;
4. the diagnostic stops after one independent exploit attempt.

This removes only the later pipe-slab placement question from the new timing
test. A nonzero `p0 result-copy self-target mutation` proves both that an skb
fragment reclaimed `base` and that PI dequeue wrote to it. A zero result is
ambiguous: either the first `mm_struct`-to-skb reclaim missed `base`, or the
Samsung 4.19 chain exited before `rt_mutex_dequeue()`.

The T870 release and a forced default-target regression release both build
with NDK r29. The fixed-size diagnostic candidate is:

```text
label:  gts7lwifi-T870XXS8DXH1-app-4.19-result-copy-self-target
size:   104128
sha256: 7476810816b71bcb5668397d7cf3b61666967ec7ddfbc86643470c782e532143
url:    artifacts/gts7lwifi-T870XXS8DXH1/cve-2026-43499-app.so
```

### 2026-08-21 19:27 combined boundary result

The device loaded the exact
`gts7lwifi-T870XXS8DXH1-app-4.19-result-copy-self-target` artifact. The
finalized private history is
`79fad7cd-5523-489e-aaff-897cdc0e46a6.json`. The app supplied
`EXPLOIT_ATTEMPTS=24`, so the supervisor overrode the target's default of one
attempt. All 24 attempts reached the combined boundary. Every reclaim socket
returned the complete 16-send, 1 MiB stream, but no byte differed from the
generated payload:

```text
slide cmp_requeue_pi ret=-1 errno=35 polls=1
slide wait_requeue_pi ret=-1 errno=110
slide result-copy trigger sentinel_cleared=1 seen_after_return=0 syscall_returned=1
sk_buff self-target readback total=1048576/1048576 sends=16 changed=0 errno=0
p0 result-copy self-target mutation=0
```

This is a valid negative result for the combined diagnostic, but it rules out
only the later pipe-slab placement question. It does not distinguish a miss in
the first `mm_struct`-to-skb reclaim from an exit before
`rt_mutex_dequeue()`. The previous stronger conclusion was incorrect.

### Restore the remaining upstream PI ordering difference

The exact upstream exploit at
`e8c777c29473455c4f4032775775ae3018d5f82a` unlocks `slide_f_pi_chain` and
enters `slide_pselect_stack_copy()` without waiting for the owner thread to
set a userspace `owner_acquired` flag. T870 currently adds that wait through
`SLIDE_WAIT_OWNER_BEFORE_PSELECT`; it is the remaining deliberate sequencing
difference in this boundary.

The 15:05 no-owner-wait run cannot be used against this correction. That run
used the subsequently rejected blocked-input pselect route. Exact ELF geometry
shows that route left the stale waiter's lock qword zero, and the retained
panic at `_raw_spin_trylock+0x1c` matches that invalid pointer. The current
result route instead provides the complete legacy waiter in the result arrays,
and its byte-exact copyout has been repeatedly verified on hardware.

The next candidate therefore changes one behavior only: remove
`SLIDE_WAIT_OWNER_BEFORE_PSELECT` for T870 and retain the single upstream-style
PI requeue, 50 ms waiter timeout, result-route layout, result-copy sentinel,
and self-target readback. It does not add a payload route or modify any kernel
offset, waiter field, priority, allocator count, or timing parameter. The
diagnostic also defines `APP_FIXED_EXPLOIT_ATTEMPTS=1` so the app's environment
cannot turn one boundary test into 24 identical exploit processes.

The T870 release and forced default-target regression release both build with
NDK r29. The support validator still accepts all 11 payload entries, and the
manifest remains repository-relative. The fixed-size hardware candidate is:

```text
label:  gts7lwifi-T870XXS8DXH1-app-4.19-upstream-owner-order
size:   104128
sha256: a28eea3c8955c45d8ba87301dde15277d41383ac507b940b8ef56366a9b81439
url:    artifacts/gts7lwifi-T870XXS8DXH1/cve-2026-43499-app.so
```

### 2026-08-21 20:06 upstream-owner-order result and reclaim correction

The finalized private history is
`ca0b74bf-c20f-48b1-ac64-ddd0e013948c.json`. The app loaded the exact
`gts7lwifi-T870XXS8DXH1-app-4.19-upstream-owner-order` artifact and honored
the target's fixed single attempt. The complete original owner ordering,
result-copy sentinel, physical-write wrapper, and self-target readback all
completed, but the returned 1 MiB socket stream was unchanged:

```text
mm leaked=ffffffc20b990800 base=ffffffc20b990000 object_index=2
mm late cpu-partial drain triggers=32
sk_buff reclaim sends=16/16 mode=1 stop_errno=0
slide result-copy trigger sentinel_cleared=1 seen_after_return=0 syscall_returned=1
p0 physical write status=0 ok=1
sk_buff self-target readback total=1048576/1048576 sends=16 changed=0 errno=0
p0 result-copy self-target mutation=0 target=ffffffc20b996180
```

Removing the owner wait therefore had no observed effect. More importantly,
the self-target diagnostic cannot establish that any skb fragment reclaimed
the freed order-3 page at `base`. It bypasses only the second-stage pipe-slab
placement. The earlier work-log statements treating `mutation=0` as proof of
an rt_mutex-chain exit are withdrawn.

### Exact Samsung 4.19 mm_struct SLUB audit

The exact stock source creates `mm_cachep` in `kernel/fork.c` with
`SLAB_HWCACHE_ALIGN | SLAB_PANIC | SLAB_ACCOUNT`; none of those flags enables
SLUB debugging for this cache. The exact defconfig enables
`CONFIG_SLUB_CPU_PARTIAL=y`, enables the debug implementation, but does not
enable `CONFIG_SLUB_DEBUG_ON`.

For T870, `MM_STRUCT_SZ=0x400`, so the exact `mm/slub.c::set_cpu_partial()`
selects `cpu_partial=6` through its `s->size >= 1024` branch. Each order-3
slab holds 32 objects. The referenced upstream exploit uses
`MM_PARTIALS=5`, producing six spray slabs, and its reclaim sequence is:

1. close all 31 `pre_ctx` memfds;
2. close the first 31 of 32 `post_ctx` memfds;
3. close one memfd from each of the six spray slabs;
4. close the shaping sockets and yield four times;
5. close `memfd_leak`;
6. immediately send the skb reclaim payloads;
7. clean up KernelSnitch and the retained prepare processes afterward.

The repository's shared implementation instead seeds the spray partials
first, splits the pre/post neighbor closes, and after closing `memfd_leak`
frees one object from each of 32 prepare slabs before sending the skbs. Those
extra 32 late-drain operations are present in the repository's initial public
commit, but they are not in the referenced upstream
`prepare_kernel_page()` and are not required by the exact T870 threshold of
six.

Decision: add a target-only `APP_UPSTREAM_MM_RECLAIM_ORDER` switch. For T870
it restores the six upstream steps above and skips the 32 prepare-slab late
drains. Other targets retain the repository's current shared behavior. This
does not change KernelSnitch counts, PI/pselect ordering, target offsets,
scheduler parameters, skb count, or the exploit route.

The T870 release and forced default-target regression release both build with
NDK r29. The support validator accepts all 11 payload entries, and the
manifest path remains repository-relative. The fixed-size candidate is:

```text
label:  gts7lwifi-T870XXS8DXH1-app-4.19-upstream-mm-reclaim
size:   104128
sha256: 95042eff164f76ed077df103de79e7382e170dcba3b01c2895d7ec0796a589b3
url:    artifacts/gts7lwifi-T870XXS8DXH1/cve-2026-43499-app.so
```

Expected hardware distinction from the preceding build:

```text
mm late cpu-partial drain triggers=0
```

The preceding `mm target-neighbor slab queued for late drain` line must be
absent. Any interpretation still requires the complete self-target readback;
this candidate tests the exact upstream first-stage reclaim, not a new exploit
or another PI timing permutation.

### 2026-08-21 19:57 upstream-mm-reclaim run: invalid before test boundary

The finalized private history is
`bebc7674-b9b8-4ff3-900c-9e26739def10.json`. It loaded the expected
`gts7lwifi-T870XXS8DXH1-app-4.19-upstream-mm-reclaim` artifact. The first P0
pipe-page preparation completed, but the second KernelSnitch run stopped after:

```text
KernelSnitch collision diagnostic stage=pileup-enter waiters=2048 bucket=128
kernel page diagnostic stage=topology-ready prepare=1024 spray=192 pre=31 post=32 leak_pid=868
```

There is no `pileup-return`, collision result, leaked `mm_struct`, reclaim
message, or self-target readback. The device rebooted at approximately 19:58
and produced `SYSTEM_LAST_KMSG_16_20260821_195858_KP`; boot ID afterward was
`798296e8-3c57-4b5a-8e9d-977c23749fe7`. The retained pstore identifies the
panic task as `cve43499-run`, but its beginning is truncated before the
initiating CPU0 exception and preserves only the other CPUs' stop traces.

This run never executed `APP_UPSTREAM_MM_RECLAIM_ORDER` and therefore cannot
accept or reject the candidate. Do not change reclaim or PI code from this
result. Repeat the same published artifact until it reaches
`mm late cpu-partial drain triggers=0` and the complete self-target readback.

### 2026-08-21 21:05 valid upstream-mm-reclaim result

The finalized private history is
`ed6c8d95-a62f-4c40-a99e-a80494c82b5b.json`. It loaded the expected build and
completed both KernelSnitch passes, the target-only upstream reclaim order,
the result-copy trigger, and the complete socket readback:

```text
mm leaked=ffffffc1026ce000 base=ffffffc1026c8000 object_index=24
mm late cpu-partial drain triggers=0
sk_buff reclaim sends=16/16 mode=1 stop_errno=0
slide result-copy trigger sentinel_cleared=1 seen_after_return=0 syscall_returned=1
sk_buff self-target readback total=1048576/1048576 sends=16 changed=0 errno=0
p0 result-copy self-target mutation=0 target=ffffffc1026ce180
```

This is the first valid negative result for the exact upstream mm-to-skb
release order. It shows that removing the repository's 32 prepare-slab late
drains did not produce a mutation on this allocation. A single result still
does not distinguish a probabilistic first reclaim miss from an rt_mutex exit.

### Restore the existing upstream page-attempt meaning

The referenced upstream `slide_leak_kernel_base()` does not treat its first
prepared page as conclusive. It loops through `SLIDE_MAX_ATTEMPTS` and performs
a fresh `prepare_good_kernel_page(PAGE_PAYLOAD_SLIDE)` before each trigger.
The current app P0 path already has an `APP_SLIDE_FRESH_PAGE_ATTEMPTS` loop,
but the self-target diagnostic unconditionally returns after its first
readback, so it cannot exercise that existing retry mechanism.

The next candidate changes no allocator order, count, skb geometry, PI field,
pselect route, or timing. Under a T870-only `APP_SLIDE_SELF_TARGET_RETRY`, an
unchanged self-target readback advances the existing fresh-page loop. It stops
immediately on a mutation. `APP_SLIDE_FRESH_PAGE_ATTEMPTS=8` is the largest
bounded sample expected to remain within the supervisor's 45-second P0 limit:
the valid first attempt took 3.890 seconds after the pipe oracle, while the
one-time pipe-oracle setup remains reused. The outer supervisor remains fixed
at one independent process so a single app run cannot expand to 24 sessions.

The T870 release and forced default-target regression release both build with
NDK r29, the support validator accepts all 11 payload entries, and the
repository-relative URL is unchanged:

```text
label:  gts7lwifi-T870XXS8DXH1-app-4.19-upstream-reclaim-retry8
size:   104128
sha256: f5aee5a238c6c3d1f69c186a15337f561d96c03e150002f7ac0d0ce23421fef7
url:    artifacts/gts7lwifi-T870XXS8DXH1/cve-2026-43499-app.so
```

### 2026-08-22 02:10 retry8 run: invalid before first payload reclaim

The finalized private history is
`eac5f28a-b6eb-481e-9c31-8fe37bce41ef.json`. It loaded the expected retry8
build. The initial 240-pipe P0 oracle completed, including its first full
KernelSnitch run, but the first payload-page preparation stopped immediately
after:

```text
p0 pipe oracle prepared base=ffffffc0ec8e8000 pipes=240 gate_slots=1
p0 diagnostic stage=kernel-page-prepare-enter pipe_base=ffffffc0ec8e8000
kernel page diagnostic stage=begin mode=1 objects_per_slab=32
```

No payload-page KernelSnitch profile, reclaim, trigger, or self-target result
was reached. The device rebooted and produced
`SYSTEM_LAST_KMSG_17_20260822_022257_KP`; the new boot ID is
`37622424-e484-4dc3-a58b-6339d00bee83`. This run is not evidence against the
retry8 reclaim candidate.

### Remove the unused pipe oracle from the self-target diagnostic

The self-target slot in `prepare_skb_payload()` derives both operands solely
from the candidate payload page:

```text
parent = direct_to_page(base)
target = payload_base + APP_SLIDE_SELF_TARGET_OFF
```

It never reads `pipebuf_page_base`; after the trigger it drains and compares
the reclaim socket directly. Nevertheless `slide_leak_physical_base()` first
runs the complete 240-pipe oracle preparation, then immediately starts a
second 1024-prepare/192-spray process topology for the actual payload page.
The latest panic occurred in that unused-oracle-to-payload transition.

The next target-only diagnostic defines
`APP_SLIDE_SELF_TARGET_SKIP_PIPE_ORACLE`. It skips only the unused first pipe
oracle when `APP_SLIDE_SELF_TARGET_DIAGNOSTIC` is active. The payload-page
KernelSnitch profile, exact upstream mm reclaim order, skb data, result-copy
trigger, PI geometry, self-target readback, and bounded eight-page retry are
unchanged. Production P0 and every other target retain the normal pipe-oracle
preparation.

The T870 release and forced default-target regression release both build with
NDK r29. The support validator accepts all 11 payload entries. The published
diagnostic candidate is:

```text
label:  gts7lwifi-T870XXS8DXH1-app-4.19-selftarget-no-pipe-retry8
size:   104128
sha256: 35c4708bc0f6251985761f2a4e6895d0c67c111ed7b76b4846bdfb79a602f4e3
url:    artifacts/gts7lwifi-T870XXS8DXH1/cve-2026-43499-app.so
```

The first distinguishing line must be:

```text
p0 self-target diagnostic skipping unused pipe oracle
```

It should be followed directly by the payload-page preparation with
`pipe_base=0000000000000000`; no `p0 pipe page diagnostic stage=begin` should
precede it.

### 2026-08-22 no-pipe retry8 hardware result

The device loaded the published
`gts7lwifi-T870XXS8DXH1-app-4.19-selftarget-no-pipe-retry8` artifact and
reached the intended diagnostic path without preparing the unused pipe
oracle:

```text
p0 self-target diagnostic skipping unused pipe oracle
```

All eight fresh `PAGE_PAYLOAD_SLIDE` preparations completed the exact
upstream mm reclaim order, the result-copy trigger, and the full reclaim
socket readback. Every attempt returned `mutation=0`. All eight independent
KernelSnitch results reported `object_index=16`, although their leaked slab
bases differed. The common terminal checkpoints were:

```text
mm late cpu-partial drain triggers=0
sk_buff reclaim sends=16/16 mode=1 stop_errno=0
slide cmp_requeue_pi ret=-1 errno=35 polls=1
slide wait_requeue_pi ret=-1 errno=110
slide result-copy trigger sentinel_cleared=1 seen_after_return=0 syscall_returned=1
p0 physical write status=0 ok=1
sk_buff self-target readback total=1048576/1048576 sends=16 changed=0 errno=0
p0 result-copy self-target mutation=0
```

This is a valid bounded negative result. Do not increase the page retry count
or change PI timing from it. It still does not distinguish failure to return
the leaked mm slab to the buddy allocator from an rt_mutex chain exit before
the self-target write.

### Live mm_struct slab geometry and next observation boundary

The connected stock device permits ADB shell to read `/proc/slabinfo` even
though both the untrusted app and ADB shell are denied the corresponding
`/sys/kernel/slab/mm_struct/*` attributes. The live cache row is:

```text
mm_struct 657 768 1024 32 8 : tunables 0 0 0 : slabdata 24 24 0
```

This proves on the running `T870XXS8DXH1` kernel that `mm_struct` has a
1024-byte object size, 32 objects per slab, and eight 4 KiB pages per slab.
The exact stored `boot.img` command line has no `slub_min_order`,
`slub_max_order`, or `slub_min_objects` override. With `CONFIG_NR_CPUS=8`,
the stock `mm/slub.c::calculate_order()` therefore agrees with the live row:
`MM_ORDER=3` is correct. The repeated `object_index=16` is not evidence for
an order-2 slab and must not be used to change `MM_ORDER`.

The next test should keep the published payload unchanged and sample the live
`mm_struct` slab row externally through ADB while one app run executes. A
decrease in total `mm_struct` slabs at the release boundary would directly
confirm the first-stage SLUB discard; no decrease would locate the failure in
the mm reclaim topology. This observation should precede any new allocator,
skb, or rt_mutex code change.

### External slab observation: target SLUB discard succeeds

ADB sampled the live `mm_struct` row while the unchanged no-pipe retry8
artifact executed. Each observed fresh-page preparation reached 55 total
slabs, then first dropped by exactly one slab before the bulk prepare-context
cleanup:

```text
1760 1760 1024 32 8 : slabdata 55 55 0
1707 1728 1024 32 8 : slabdata 54 54 0
1514 1536 1024 32 8 : slabdata 48 48 0
1306 1344 1024 32 8 : slabdata 42 42 0
1082 1120 1024 32 8 : slabdata 35 35 0
 858  896 1024 32 8 : slabdata 28 28 0
 656  768 1024 32 8 : slabdata 24 24 0
```

In the exact upstream release order, the pre, post, and six spray closes make
partial slabs; only closing `memfd_leak` makes the target slab empty. The
single 55-to-54 total-slab transition therefore confirms that the leaked
target slab is discarded from the mm cache before the later prepare cleanup.
Do not change `MM_ORDER`, `MM_PARTIALS`, or the upstream reclaim order from
this result. The remaining boundary is buddy-to-order-3-skb reuse or the
subsequent rt_mutex chain.

### Exact 4.19 fake-owner exit and full-FOPS gate candidate

The T870 profile inherited `SLIDE_LOCK_OWNER_VALUE=1` in its initial target
commit. In the exact Samsung 4.19 source, `rt_mutex_owner(lock)` masks bit
zero, so that value is a NULL owner with the waiter flag set. After step 7
requeues the stale waiter's lock-tree node,
`rt_mutex_adjust_prio_chain()` reaches its step 9 owner check and returns.
It cannot execute step 11's `rt_mutex_dequeue_pi()`, where the payload's
crafted `pi_tree_entry` and physical-write operands are consumed.

The repository already has a full-FOPS geometry for this owner chain. The
T870 gate now reuses that existing construction: `owner=fake_task|1`,
`waiter.task=init_task`, `fake_task.sched_task_group=root_task_group`, an
empty fake-task PI root, and `pi_top_task=init_task`. No allocator, skb,
KernelSnitch, pselect, result-copy, offset, or timing value changes. Because
this is the first T870 run that can enter the deeper owner path, the diagnostic
is limited to one fresh page.

The T870 release and the default `pa3q-S938NKSUACZF1` regression release
both build with NDK r29. The support feed remains repository-relative and its
T870 size remains 104128 bytes:

```text
label:  gts7lwifi-T870XXS8DXH1-app-4.19-selftarget-full-fops-owner
size:   104128
sha256: f349eb7e5f25c71dd8eab4dc470648eb567441fb2ab263c79f2e289bb99a90c5
url:    artifacts/gts7lwifi-T870XXS8DXH1/cve-2026-43499-app.so
```
