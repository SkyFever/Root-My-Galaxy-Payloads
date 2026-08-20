#!/usr/bin/env python3
"""Recover an arm64 CONFIG_MODVERSIONS Module.symvers from a target vmlinux."""

import argparse
import struct
from pathlib import Path

from elftools.elf.elffile import ELFFile


def read_virtual_address(elf_file, address, size):
    for segment in elf_file.iter_segments():
        if segment["p_type"] != "PT_LOAD":
            continue
        start = segment["p_vaddr"]
        end = start + segment["p_filesz"]
        if start <= address and address + size <= end:
            stream = elf_file.stream
            stream.seek(segment["p_offset"] + address - start)
            return stream.read(size)
    raise ValueError(f"virtual address 0x{address:x} is not backed by a PT_LOAD segment")


def read_c_string(elf_file, address, max_size=4096):
    for segment in elf_file.iter_segments():
        if segment["p_type"] != "PT_LOAD":
            continue
        start = segment["p_vaddr"]
        end = start + segment["p_filesz"]
        if not start <= address < end:
            continue
        size = min(max_size, end - address)
        data = read_virtual_address(elf_file, address, size)
        nul = data.find(b"\0")
        if nul < 0:
            raise ValueError(f"unterminated string at virtual address 0x{address:x}")
        return data[:nul].decode("ascii")
    raise ValueError(f"virtual address 0x{address:x} is not backed by a PT_LOAD segment")


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("vmlinux", type=Path)
    parser.add_argument("output", type=Path)
    args = parser.parse_args()

    with args.vmlinux.open("rb") as stream:
        elf_file = ELFFile(stream)
        if not elf_file.little_endian or elf_file.elfclass != 64 or elf_file["e_machine"] != "EM_AARCH64":
            raise ValueError("only little-endian ELF64 arm64 kernels are supported")

        symbol_table = elf_file.get_section_by_name(".symtab")
        if symbol_table is None:
            raise ValueError("vmlinux has no .symtab")

        symbols = {symbol.name: symbol["st_value"] for symbol in symbol_table.iter_symbols()}
        ranges = (
            (
                symbols["__start___ksymtab"],
                symbols["__stop___ksymtab"],
                symbols["__start___kcrctab"],
                symbols["__stop___kcrctab"],
                "EXPORT_SYMBOL",
            ),
            (
                symbols["__start___ksymtab_gpl"],
                symbols["__stop___ksymtab_gpl"],
                symbols["__start___kcrctab_gpl"],
                symbols["__stop___kcrctab_gpl"],
                "EXPORT_SYMBOL_GPL",
            ),
        )

        table_ranges = []
        for table_start, table_stop, crc_start, crc_stop, export_type in ranges:
            table_size = table_stop - table_start
            crc_size = crc_stop - crc_start
            if crc_size % 4:
                raise ValueError(f"unaligned {export_type} CRC table")
            symbol_count = crc_size // 4
            if symbol_count == 0 or table_size % symbol_count:
                raise ValueError(f"cannot derive {export_type} kernel_symbol size")
            entry_size = table_size // symbol_count
            if entry_size not in (8, 12, 16):
                raise ValueError(
                    f"unsupported {export_type} kernel_symbol size: {entry_size}"
                )
            table_ranges.append(
                (table_start, table_stop, crc_start, export_type, entry_size, symbol_count)
            )

        entries = []
        for table_start, _, crc_start, export_type, entry_size, symbol_count in table_ranges:
            for index in range(symbol_count):
                entry_address = table_start + index * entry_size
                if entry_size in (8, 12):
                    name_field_address = entry_address + 4
                    name_offset = struct.unpack(
                        "<i", read_virtual_address(elf_file, name_field_address, 4)
                    )[0]
                    name_address = name_field_address + name_offset
                else:
                    name_address = struct.unpack(
                        "<Q", read_virtual_address(elf_file, entry_address + 8, 8)
                    )[0]
                exported_name = read_c_string(elf_file, name_address)
                crc_address = crc_start + index * 4
                crc = struct.unpack("<I", read_virtual_address(elf_file, crc_address, 4))[0]
                entries.append((exported_name, crc, export_type))

    expected_count = sum(symbol_count for *_, symbol_count in table_ranges)
    if len(entries) != expected_count:
        raise ValueError(f"recovered {len(entries)} symbols, expected {expected_count}")
    unique_entries = {}
    for name, crc, export_type in entries:
        previous = unique_entries.get(name)
        if previous is not None and previous != (crc, export_type):
            raise ValueError(f"conflicting duplicate export for {name}")
        unique_entries[name] = (crc, export_type)
    entries = [(name, *values) for name, values in unique_entries.items()]

    entries.sort(key=lambda entry: entry[0])
    args.output.write_text(
        "".join(f"0x{crc:08x}\t{name}\tvmlinux\t{export_type}\t\n" for name, crc, export_type in entries),
        encoding="utf-8",
        newline="\n",
    )
    print(f"wrote {len(entries)} exact target CRCs to {args.output}")


if __name__ == "__main__":
    main()
