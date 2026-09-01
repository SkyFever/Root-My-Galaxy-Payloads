# Galaxy Z Flip5 SM-F731N / F731NKSS5EYGB port record

This record documents the static inputs and derived values for the Galaxy Z
Flip5 (`b5q`, Snapdragon 8 Gen 2 / SM8550) profile
`b5q-F731NKSS5EYGB`. The connected target runs Samsung's
`android13-5.15` kernel at 5.15.153 with KDP, RKP, DEFEX, CFI, Shadow Call
Stack, PAC, and MTE/HW-tag KASAN support.

This profile is diagnostic-only until it passes the connected-device runtime
gate. It must not be added to `support/targets-v3.json` or published before
that gate.

## Target and donor identity

Connected-device ground truth:

```text
model: SM-F731N
device/product: b5q / b5qksx
display build: AP3A.240905.015.A2.F731NKSS5EYGB
fingerprint: samsung/b5qksx/b5q:15/AP3A.240905.015.A2/F731NKSS5EYGB:user/release-keys
security patch: 2025-08-01
kernel release: 5.15.153-android13-8-30958511-abF731NKSS5EYGB
SoC / ABI: SM8550 / arm64-v8a
```

The supplied AP/BL package is `F731NTBS5EYGB`. The device owner confirmed
that TBS versus KSS is an edition distinction with no exploit or entry-point
difference. TBS is therefore the static-analysis donor; KSS remains the
runtime identity. The KSS runtime still independently gates the trace event,
caller anchors, BTF size, kernel lineage, and every exploit checkpoint.

## Extracted artifacts

```text
donor kernel: 5.15.153-android13-8-30958511-abF731NTBS5EYGB
boot.img SHA-256:
  0cdb18cf65c1f97de1fbc85ca933ec27f6c097d260c2947672cf2685afc6c3ff
raw Image SHA-256:
  68f75db301a1afcf7e4ee631b3530e829834836d093ce49dff4c7cbc3928e6a6
recovered ELF SHA-256:
  f7882e47c9fafabad2273abd238b257f42704da1cb278be45bd6a455e439fc99
raw BTF SHA-256:
  bd3ca91839083211f18d1921c72be4f3eac97a403225ff1ca0997c787597624b
raw BTF size: 5,948,645 bytes
LinuxLoader SHA-256:
  5d0d512c3cf8f29bd5f8078e0821782d7e804ce8346723d08ec84630efb6f830
```

The raw BTF size exactly matches `/sys/kernel/btf/vmlinux` on the KSS
device.

## Physical load proof

The boot header reports Image `text_offset == 0`. LinuxLoader contains the
SM8550 kernel load delta `0x80000`, and all four extracted vendor-boot DTBs
contain `gunyah_hyp_region@80000000`. Therefore:

```text
P0_PHYS_OFFSET      = 0x80000000
P0_KERNEL_PHYS_LOAD = 0x80080000
KIMAGE_TEXT_BASE    = 0xffffffc008000000
```

## Runtime inventory

Read-only checks on the KSS device established:

- 4 KiB pages and 39-bit arm64 VA.
- `sched_blocked_reason` trace event ID 108.
- `/dev/ashmem` exists with mode 0666.
- tracefs and configfs are mounted.
- `mm_struct` slab geometry is object size `0x400`, 32 objects per slab,
  order 3. BTF reports C size `0x3e0`.
- The boot configuration does not enable randomized kernel-stack offsets.
- `kasan=off` is present in the effective boot arguments, so the target uses
  non-MTE KernelSnitch mode even though MTE/HW-tag KASAN support is compiled
  in.
- A one-second tracefs diagnostic was restored to the original
  `enable=0`, `tracing_on=0` state and repeatedly produced
  `worker_thread+0x78/0x738`.

No exploit or payload was executed while collecting this inventory.

## F731N route derivation

The F731N route is selected from the target kernel disassembly. It enables the
repository's shared tracefs/MCAST implementation (`SLIDE_STACK_WRITER=1`),
but does not import another target's offsets or timing table. The target stack
overlay is:

```text
futex waiter from syscall-entry SP:
  __arm64_sys_futex 0x80
  + do_futex 0x140
  + futex_wait_requeue_pi 0x1b0
  - waiter local 0x98
  = SP - 0x2d8

MCAST userspace-copy base from syscall-entry SP:
  __arm64_sys_setsockopt 0x10
  + __sys_setsockopt 0x70
  + sock_common_setsockopt 0x10
  + ipv6_setsockopt 0x40
  + do_ipv6_setsockopt local 0x280
  = SP - 0x350

waiter relative to copy base:
  0x350 - 0x2d8 = 0x78
```

`do_ipv6_setsockopt` copies the full `0x108`-byte userspace stamp, so the
waiter lies inside that copy. The profile therefore uses
`MCAST_WAITER_OFF=0x78`; it does not define or consume
`SLIDE_PSELECT_WORD_SHIFT`.

The vfork unwind anchor is the saved return immediately after
`bl wait_for_common` in `wait_for_vfork_done`:
`0xffffffc0080c8fb4`, yielding
`SLIDE_TRACEFS_VFORK_CALLER_OFF=0x000c8fb4`.
The worker anchor is the return after `bl schedule` in `worker_thread`:
`0xffffffc00810d370`, yielding
`SLIDE_TRACEFS_WORKER_CALLER_OFF=0x0010d370`. The latter is independently
confirmed by the KSS tracefs diagnostic.

BTF reports `sizeof(struct skb_shared_info) == 344`; the target therefore uses
`SKB_DATA_DELTA=-0xe80` and send size `0x8e80`.

## Recovered symbols

All offsets are relative to `0xffffffc008000000`.

| Macro/use | Recovered symbol or object | Offset |
| --- | --- | ---: |
| `INIT_TASK_OFF` | `init_task` | `0x02ac9bc0` |
| `PREPARE_KERNEL_CRED_OFF` | `prepare_kernel_cred` | `0x0011db04` |
| `COMMIT_CREDS_OFF` | `commit_creds` | `0x0011f840` |
| `OVERRIDE_CREDS_OFF` | `override_creds` | `0x0011e918` |
| `ROOT_TASK_GROUP_OFF` | `root_task_group` | `0x02b79ac0` |
| `SELINUX_ENFORCING_OFF` | `selinux_state.enforcing` | `0x02c4e438` |
| `KMALLOC_CACHES_OFF` | KSS runtime-confirmed cache table | `0x01f77590` |
| `ANON_PIPE_BUF_OPS_OFF` | `anon_pipe_buf_ops` | `0x01da2f20` |
| `SYSTEM_UNBOUND_WQ_OFF` | `system_unbound_wq` | `0x0295e480` |
| `CALL_USERMODEHELPER_EXEC_WORK_OFF` | `call_usermodehelper_exec_work` | `0x00103e50` |
| `ASHMEM_FOPS_OFF` | `ashmem_fops` | `0x01f21050` |
| `ASHMEM_MISC_FOPS_OFF` | `ashmem_misc + 0x10` | `0x02ac1b88` |
| `ASHMEM_IOCTL_OFF` | `ashmem_ioctl` | `0x010b5dcc` |
| `ASHMEM_COMPAT_IOCTL_OFF` | compat ashmem ioctl | `0x010b6428` |
| `ASHMEM_MMAP_OFF` | `ashmem_mmap` | `0x010b6480` |
| `ASHMEM_OPEN_OFF` | `ashmem_open` | `0x010b6760` |
| `ASHMEM_RELEASE_OFF` | `ashmem_release` | `0x010b67f8` |
| `ASHMEM_SHOW_FDINFO_OFF` | `ashmem_show_fdinfo` | `0x010b6914` |
| `CONFIGFS_READ_ITER_OFF` | `configfs_read_iter` | `0x005d30a0` |
| `CONFIGFS_BIN_WRITE_ITER_OFF` | `configfs_bin_write_iter` | `0x005d3ac8` |
| `COPY_SPLICE_READ_OFF` | `generic_file_splice_read` | `0x00523f68` |
| `NOOP_LLSEEK_OFF` | `noop_llseek` | `0x004b6d9c` |
| slide logger name | `"nfnetlink_log"` | `0x01c96bb0` |
| slide logger object | `nfulnl_logger` | `0x02961dc0` |
| boot-id data slot | `random_table[boot_id].data` | `0x02a7f960` |
| boot-id value | `sysctl_bootid` | `0x02ceaf29` |

## BTF layouts

The target header uses the recovered F731N layouts, including:

- `task_struct` size `0x1200`: `usage=0x38`, `prio=0x7c`,
  `normal_prio=0x84`, `sched_task_group=0x400`,
  `real_cred=0x790`, `cred=0x798`, `pi_lock=0x884`,
  `pi_waiters=0x898`, `pi_top_task=0x8a8`,
  `pi_blocked_on=0x8b0`.
- compact `rt_mutex_waiter` size `0x58`: `pi_tree_entry=0x18`,
  `task=0x30`, `lock=0x38`, `wake_state=0x40`, `prio=0x44`,
  `deadline=0x48`, `ww_ctx=0x50`.
- `file_operations` size `0x120`: ioctl `0x50`, compat `0x58`,
  mmap `0x60`, open `0x70`, release `0x80`, splice `0xc8`,
  show-fdinfo `0xe0`.
- `pipe_buffer` size `0x28`; `struct page` size `0x40`.
- `workqueue_struct.dfl_pwq=0xb0`,
  `worker_pool.worklist=0x20`, `worker_pool.nr_idle=0x34`.

## P0 fingerprint

The repository's official `tools/generate_p0_fingerprint.pl` generated
`src/targets/b5q-F731NKSS5EYGB/p0_fingerprint.h` from the donor raw Image at
probe offset `0x1f0000`. All 32 slide rows and 256 source qwords passed an
independent readback.

```text
p0_fingerprint.h SHA-256:
160fa0d486fefe8f5af157a129a56327252f6658f6edaf47dbc7fce48638a047
```

## KSS runtime-confirmed cache table

The KSS device read-only diagnostic observed the same 2 KiB pipe slab pointer
on independently reclaimed pipe pages and found the normal/accounted cache
slot geometry at:

```text
table base: ffffffc00a117590
image offset: 0x01f77590
normal1k: ffffff80011d2780
normal2k: ffffff80011d2900
cgroup1k: ffffff80011d2780
cgroup2k: ffffff80011d2900
```

The target therefore uses `KMALLOC_CACHES_OFF=0x01f77590`. This value comes
from the connected KSS device and is not copied from another profile.

## KernelSU exact-source build

The F731N EYGB module was built from Samsung's released
`SM-F731N_KOR_15_Opensource/kernel_platform/common` tree, not repackaged from
the DYI3 module. Device execution has so far covered only the userspace binary
and embedded-asset extraction; module initialization remains untested.

- The connected device's `/proc/config.gz` was applied; the only resulting
  config delta was `CONFIG_PAHOLE_VERSION`.
- The build used Android clang `r450784e`, matching the running kernel.
- The module carries exact
  `5.15.153-android13-8-30958511-abF731NKSS5EYGB` vermagic.
- All 200 undefined KO symbols exist in the generated target symbol list.
- 64 symbols intentionally use the manual kallsyms path.
- `__versions` is zero-length and target CRC mismatches are zero.
- `CONFIG_KSU_SAMSUNG_NO_PATCH_TEXT=y` is set for the Samsung KDP/RKP path.
- A clean KernelSU v3.2.5 (`b0bc817`) userspace build embeds that KO as
  `android13-5.15_kernelsu.ko`.

```text
android13-5.15.153_kernelsu-b5q-F731NKSS5EYGB-kdp.ko
  size: 357048
  SHA-256: 07024ab7e3b836d53b1f6902bcace67e7175358b4e68d568ad67f17d555237f6

ksud-b5q-F731NKSS5EYGB-kdp
  size: 4775552
  SHA-256: 54db10df723877729410ea1809e8cac3f911dcf78f5b7b45dbe41b876fefc3a1
```

This remains hardware-untested. Late-loading is
RAM-only and does not write boot, vbmeta, or Knox state; a reboot restores the
stock kernel. It can still panic the running kernel if a runtime-only ABI
difference exists, so successful exploit and module-load checkpoints must be
evaluated separately.

## Build and publication gates

The verified build commands use the repository's existing targets:

```sh
make TARGET=b5q-F731NKSS5EYGB ANDROID_NDK_HOME=... all
make TARGET=b5q-F731NKSS5EYGB ANDROID_NDK_HOME=... release
```

NDK r29 (`29.0.14206865`) produced:

```text
cve-2026-43499-app.so
  size: 132064
  SHA-256: 544e5fb2bfd5994d0115995d528836057293cfee9e362bcaf51f62bf8d702670

cve-2026-43499-app.release.so
  size: 104128
  SHA-256: 9765c2f935029dd34a439ca4030cc9009b1678d70955a38341dd23988e8e2be9
```

Both are AArch64 Android API 35 shared objects. The release object is stripped,
retains a valid dynamic and section table after fixed-size padding, embeds only
the F731N variant label, and was compiled with `SLIDE_STACK_WRITER=1`.
All direct symbol macros, `ashmem_misc+0x10`, the logger and boot-id nested
pointers, and the BTF layouts were revalidated against the recovered artifacts.

The app requires a remote support entry before it can select an exact profile.
The F731N entry and artifacts are therefore published as runtime-gated
candidates, not as a completed support claim.

Before the first connected-device run:

1. Confirm the relative feed paths and actual artifact sizes once more.
2. Commit and push the isolated `f731n-eygb` branch to the SkyFever fork.
3. Run `C:\Users\SkyFever\Documents\adb_clear_payload.ps1` exactly once.
4. Verify the app selects `b5q-F731NKSS5EYGB` and downloads both hashes above.
5. Run the official app route and preserve its last printed checkpoint.
6. Judge exploit/root and KernelSU late-load separately.
7. If the kernel panics, allow reboot and re-check green boot state, Knox
   Warranty Bit, verity, and SELinux before changing any target constant.

No boot, vbmeta, or persistent partition write is authorized by this candidate
flow. Resident-agent work remains out of scope until the runtime gate passes.

## First hardware result and checkpoint derivative

The clean release from main commit `eb017bd` was executed once. The device
downloaded the exact published payload and ksud hashes. The app's persisted
record ended with:

```text
[+] slide-kaslr-ok source=forced ... p0_offset=00020000
[*] controlled mm group attempt=24 group=0 ... count=24
```

The following boot created
`SYSTEM_LAST_KMSG_30_20260902_004034_KP`. The shell-visible entry reports
`Last boot reason: reboot` but exposes no kernel fault PC or call trace. No
selected-group, reclaim, writer, fops, root, or KernelSU checkpoint is present.
After reboot, verified boot is green, the bootloader lock property is 1,
verity is enforcing, SELinux is Enforcing, and the warranty-bit property is 0.

The derivative build changes no target address, layout, reclaim count, timing,
writer, root, or KernelSU input. It enables b5q-only unbuffered log lines
immediately before and after `controlled_mm_leak()` once the largest collected
group reaches 24 objects. The clean port remains preserved at `ce90ee3`.
