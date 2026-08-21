# SM-T870 / T870XXS8DXH1 target

Exact firmware:

```text
samsung/gts7lwifixx/gts7lwifi:13/TP1A.220624.014/T870XXS8DXH1:user/release-keys
Linux 4.19.113-27114284 #1 SMP PREEMPT Mon Aug 5 15:36:53 +07 2024
```

The target values come from the exact Samsung release source, stock boot
Image, recovered symbols and relocations, plus read-only device data.

The repository exploit flow is unchanged apart from target-gated 4.19
compatibility branches:

- configfs uses legacy `file_operations.read/write`;
- the 0x50-byte legacy waiter overlaps the pselect result fd-set at shift 1;
- the result-copy route triggers while `core_sys_select` still owns its stack,
  rather than after `__arm64_sys_pselect6` has reused that stack.

The result-copy geometry comes from the exact stock ELF. `__arm64_sys_futex`
uses `0x70` bytes, `do_futex` uses `0x1e0`, and its waiter is at `sp+0xc0`.
`__arm64_sys_pselect6` uses `0xa0`, while `core_sys_select` uses `0x1c0` and
places its stack fd-set storage at `sp+0x50`. With 320 fds (five qwords per
set), the stale waiter begins at internal qword 16, one qword into the result
read set. A non-ready sentinel in unused input qword zero lets the consumer
issue the original `sched_setattr` trigger as soon as result copy-out starts,
before the syscall wrapper can overwrite the waiter.

Verified corrections versus the discarded experimental target are:

- `WQ_DFL_PWQ_OFF=0xc0`;
- `POOL_WORKLIST_OFF=0x28`;
- `POOL_NR_IDLE_OFF=0x3c`;
- `COPY_SPLICE_READ_OFF=0x0028e26c` (`generic_file_splice_read`).

The stock kernel forces module signatures and enables Samsung DEFEX while the
bootloader reports a locked verified-boot state. Before the unchanged UMH
stage invokes `ksud late-load`, this target alone clears `sig_enforce` and sets
the exact `boot_state_unlocked` byte through the exploit's existing kernel data
write primitive. Other targets do not compile these writes.

The app payload and the dedicated KernelSU v3.2.5 4.19 pair are build- and
static-audit verified. Hardware execution remains pending.
