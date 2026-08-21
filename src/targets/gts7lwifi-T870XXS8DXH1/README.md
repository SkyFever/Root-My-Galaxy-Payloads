# SM-T870 / T870XXS8DXH1 target

Exact firmware:

```text
samsung/gts7lwifixx/gts7lwifi:13/TP1A.220624.014/T870XXS8DXH1:user/release-keys
Linux 4.19.113-27114284 #1 SMP PREEMPT Mon Aug 5 15:36:53 +07 2024
```

The target values come from the exact Samsung release source, stock boot
Image, recovered symbols and relocations, plus read-only device data.

The repository exploit flow is unchanged. Two target-gated 4.19 compatibility
branches are enabled:

- configfs uses legacy `file_operations.read/write`;
- the 0x50-byte legacy waiter overlaps the pselect result fd-set at shift 1.

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
