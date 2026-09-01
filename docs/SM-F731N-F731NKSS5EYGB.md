# SM-F731N / F731NKSS5EYGB clean port record

This document defines a clean port of `b5q-F731NKSS5EYGB` from the procedure
in [`PORTING.md`](PORTING.md) and the module requirements in
[`../kernelsu/README.md`](../kernelsu/README.md).

## Status

The port is not implemented and is not supported yet.

All earlier F731N notes, target headers, offsets, fingerprints, payloads,
KernelSU modules, late-load binaries, build logs, and checkpoint derivatives
are rejected as inputs to this port. They may contain assumptions inherited
from `dm1q`. No value from those records may be copied into the new target.

The only accepted starting facts in this document are values read again from
the connected device during this clean pass and the requirements stated by the
two governing repository documents.

## 1. Fresh runtime identity

Read directly from the connected device:

```text
model: SM-F731N
device: b5q
product: b5qksx
display build: AP3A.240905.015.A2.F731NKSS5EYGB
fingerprint: samsung/b5qksx/b5q:15/AP3A.240905.015.A2/F731NKSS5EYGB:user/release-keys
security patch: 2025-08-01
kernel release: 5.15.153-android13-8-30958511-abF731NKSS5EYGB
page size: 4096
sched_blocked_reason event ID: 108
```

The device shell cannot read `/sys/kernel/btf/vmlinux`; both its size and
contents returned `Permission denied`. BTF must therefore be extracted and
validated from the exact firmware kernel as described in `PORTING.md`.

The following configuration lines were read again from the connected device's
`/proc/config.gz`:

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

These runtime readings establish target identity and build constraints only.
They do not validate an exploit route, target offset, or KernelSU artifact.

## 2. Target isolation rule

The new target directory is:

```text
src/targets/b5q-F731NKSS5EYGB/
```

`dm1q` is not a donor or compatibility baseline. Sharing Android 13, KMI
`android13-5.15`, or kernel version `5.15.153` does not validate another
device's values.

Shared exploit code may be used only through the interface already defined by
the repository. Every firmware-dependent constant and layout must be derived
from F731N evidence and placed in the F731N target directory. Shared exploit
code must not be changed merely to make an unverified F731N value appear to
work.

## 3. Firmware-to-target procedure

Perform these steps in order. Record the command, source file, result, and
SHA-256 for every generated artifact.

### 3.1 Exact firmware

1. Obtain the exact AP, BL, CSC, and CP identity for the connected build.
2. Extract `boot.img` and `vendor_boot.img` from that exact package.
3. Extract the raw ARM64 Image from `boot.img` using its actual boot-header
   version and kernel-size field.
4. Record the exact firmware identity, file sizes, and SHA-256 values.

Do not accept a prior extraction or a similarly named edition until its binary
identity has been checked against the clean target inputs.

### 3.2 ELF, symbols, and BTF

1. Recover `vmlinux.elf` from the clean raw Image.
2. Generate a numeric symbol list from that ELF.
3. Locate raw BTF by validating the complete header, section bounds, and the
   required initial NUL in the string section.
4. Require exactly one valid BTF candidate.
5. Generate both raw and C BTF dumps.
6. Derive numeric structure sizes, member bit offsets, and bitfield widths from
   the raw dump only.

Record every symbol used by the payload as an offset from the recovered F731N
image base. Record derived objects, such as a `miscdevice` member, as the base
symbol plus the F731N BTF member offset.

### 3.3 Physical load

Derive `P0_PHYS_OFFSET` and `P0_KERNEL_PHYS_LOAD` from the exact F731N BL and
ARM64 Image. Record the bootloader instruction sequence and Image
`text_offset` that prove the values. Do not reuse another target's physical
mapping.

### 3.4 Slide and stack data

The runtime trace event ID is `108`, but all remaining slide data is still
underived in this clean port.

1. derive the worker caller from the instruction after the blocking
   `bl schedule` in the F731N `worker_thread`;
2. verify observed callers produce an aligned slide within the supported P0
   range;
3. derive the stack overlay from F731N syscall disassembly and F731N BTF;
4. prove every overwritten waiter member lands at the intended F731N offset;
5. generate all P0 rows from the clean raw Image and read back all 256 source
   qwords.

No earlier `pselect`, MCAST, waiter-offset, timing, reclaim, or allocator value
is accepted.

### 3.5 Target files and payload

Create:

```text
src/targets/b5q-F731NKSS5EYGB/target.h
src/targets/b5q-F731NKSS5EYGB/p0_fingerprint.h
```

Build only after every macro in `target.h` has a recorded F731N derivation:

```sh
make TARGET=b5q-F731NKSS5EYGB \
  ANDROID_NDK_HOME=/path/to/android-ndk-r29 release
```

The release artifact destination is:

```text
artifacts/b5q-F731NKSS5EYGB/cve-2026-43499-app.so
```

Record its size and SHA-256 only after the clean build succeeds.

## 4. Exact F731N KernelSU procedure

No existing F731N KO or `ksud` is accepted. Rebuild both from clean inputs.

1. Start from KernelSU v3.2.5 commit
   `b0bc817b4e966aa6aa830834eaf6ef765d821d40`.
2. Apply the Samsung KDP/RKP/DEFEX patch cleanly.
3. Use the exact F731N Samsung open-source kernel corresponding to the running
   release and the freshly read target configuration.
4. Set `UTS_RELEASE` and `include/config/kernel.release` to the full exact
   F731N kernel release.
5. Build with the compiler required by the F731N source release.
6. Run `check_symbol` against the clean recovered F731N `vmlinux.elf`.
7. Inspect `CONFIG_MODULE_FORCE_LOAD`, `CONFIG_MODVERSIONS`, and
   `CONFIG_TRIM_UNUSED_KSYMS` before choosing the loader contract.
8. Do not import `Module.symvers` from another device or GKI build.
9. If manual relocation is required, retain `.symtab` and `.strtab`, require a
   zero-length `__versions` section, and prove that every undefined import
   resolves in the recovered F731N target.
10. Make `modinfo` report the full exact F731N vermagic.
11. Strip debug sections only, embed the resulting KO as the exact
    `android13-5.15_kernelsu.ko` asset, and force a clean `ksud` rebuild.
12. Publish the versioned standalone KO and the `ksud` embedding that exact KO
    as one inseparable pair.

The module is not validated until the exact-source build is reproducible, the
static audits pass, and the connected F731N completes late-load without panic
or reboot.

## 5. Support feed procedure

Add `b5q-F731NKSS5EYGB` to `support/targets-v3.json` only after the clean
payload and clean KernelSU pair exist.

1. use the verified runtime model list and leading three-part `uname -r`;
2. use repository-relative artifact paths;
3. do not add the removed
   `https://raw.githubusercontent.com/BuSung-dev/Root-My-Galaxy-Payloads/main/`
   prefix;
4. set sizes and SHA-256 values from the final files;
5. validate the JSON and verify that every relative path resolves;
6. confirm Root My Galaxy parses and selects the exact profile.

A feed entry enables testing. It is not proof of support.

## 6. Hardware gates

Evaluate the payload and KernelSU as separate stages.

1. clear the app payload cache once before each new published build;
2. verify the selected profile and downloaded hashes;
3. preserve the last printed checkpoint;
4. require an explicit root-success checkpoint before starting KernelSU;
5. require successful module initialization and Manager access;
6. treat panic, reboot, app failure, or timeout as failure;
7. after reboot, verify the stock boot state, bootloader lock, verity, SELinux,
   and Knox Warranty Bit;
8. do not mark the profile complete until both stages pass.

Do not retry a stack writer on the same boot after it has executed. Do not use
failed-run behavior to invent or tune target constants; return to the first
unproven derivation required by `PORTING.md`.

## 7. Clean evidence checklist

The following fields are intentionally empty until re-derived:

```text
exact firmware package and hashes: pending
raw Image hash and header values: pending
recovered ELF hash and image base: pending
validated BTF interval and hash: pending
target symbol offsets: pending
target structure layouts: pending
physical load proof: pending
trace caller anchors: pending
stack overlay proof: pending
P0 fingerprint hash and readback: pending
payload size and SHA-256: pending
KernelSU source revision and patch result: pending
KernelSU symbol/CRC audit: pending
KO vermagic, size, and SHA-256: pending
ksud embedded-asset proof, size, and SHA-256: pending
root hardware gate: pending
KernelSU hardware gate: pending
stock-integrity recheck: pending
```

