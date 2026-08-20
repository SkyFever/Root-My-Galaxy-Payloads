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
label: gts7lwifi-T870XXS8DXH1-app-in-kernel-cleanup-checkpoint
artifact SHA-256: 77a93086fc93ffacceb0dff4495ab300f1510d7fc017b5dda526c3f96f3d1e3f
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
