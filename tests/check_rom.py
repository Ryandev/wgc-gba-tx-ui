#!/usr/bin/env python3
"""Validate that a built .gba ROM and its .elf meet real-hardware GBA BIOS and cart invariants.

    python3 tests/check_rom.py <rom.gba> <rom.elf> [<rom2.gba> <rom2.elf> ...]

mGBA's built-in HLE BIOS boots ROMs even if the Nintendo logo, fixed 0x96 byte, or header complement
checksum are corrupt, whereas a real GBA locks up on the boot screen. This check verifies:
  1. GBA cartridge header (0x00..0xBF): ARM branch entry point, 156-byte Nintendo logo, title/code/maker,
     fixed byte 0x96, zeroed reserved fields, and header complement checksum at 0xBD.
  2. Binary layout: 4-byte aligned, fits in one 64 KB flash sector, ends with ld/gba_cart.ld's dkARM pad.
  3. ELF layout: __rom_end__ matches the .gba file size, all ALLOC sections sit in ROM/IWRAM/EWRAM,
     hot routines (isr_master, memcpy32, memset32, canvas_*) are in IWRAM, link_write stays in ROM,
     and static IWRAM/EWRAM usage stays within budget.
"""
import os
import struct
import subprocess
import sys

LOGO = bytes([
    0x24, 0xFF, 0xAE, 0x51, 0x69, 0x9A, 0xA2, 0x21, 0x3D, 0x84, 0x82, 0x0A, 0x84, 0xE4, 0x09, 0xAD,
    0x11, 0x24, 0x8B, 0x98, 0xC0, 0x81, 0x7F, 0x21, 0xA3, 0x52, 0xBE, 0x19, 0x93, 0x09, 0xCE, 0x20,
    0x10, 0x46, 0x4A, 0x4A, 0xF8, 0x27, 0x31, 0xEC, 0x58, 0xC7, 0xE8, 0x33, 0x82, 0xE3, 0xCE, 0xBF,
    0x85, 0xF4, 0xDF, 0x94, 0xCE, 0x4B, 0x09, 0xC1, 0x94, 0x56, 0x8A, 0xC0, 0x13, 0x72, 0xA7, 0xFC,
    0x9F, 0x84, 0x4D, 0x73, 0xA3, 0xCA, 0x9A, 0x61, 0x58, 0x97, 0xA3, 0x27, 0xFC, 0x03, 0x98, 0x76,
    0x23, 0x1D, 0xC7, 0x61, 0x03, 0x04, 0xAE, 0x56, 0xBF, 0x38, 0x84, 0x00, 0x40, 0xA7, 0x0E, 0xFD,
    0xFF, 0x52, 0xFE, 0x03, 0x6F, 0x95, 0x30, 0xF1, 0x97, 0xFB, 0xC0, 0x85, 0x60, 0xD6, 0x80, 0x25,
    0xA9, 0x63, 0xBE, 0x03, 0x01, 0x4E, 0x38, 0xE2, 0xF9, 0xA2, 0x34, 0xFF, 0xBB, 0x3E, 0x03, 0x44,
    0x78, 0x00, 0x90, 0xCB, 0x88, 0x11, 0x3A, 0x94, 0x65, 0xC0, 0x7C, 0x63, 0x87, 0xF0, 0x3C, 0xAF,
    0xD6, 0x25, 0xE4, 0x8B, 0x38, 0x0A, 0xAC, 0x72, 0x21, 0xD4, 0xF8, 0x07,
])

MAX_ROM_BYTES = 65536     # fits in a single 64 KB sector of the 4 MB MX29LV320
MAX_IWRAM_STATIC = 8192   # <= 8 KB static IWRAM, leaving >= 24 KB for stacks below 0x03007F00
MAX_EWRAM_STATIC = 262144 # 256 KB EWRAM
PAD_TRAILER = b'dkARM\x00\x00\x00'

IWRAM_REQUIRED = (
    'isr_master', 'memcpy32', 'memset32',
    'canvas_clear', 'canvas_pixel', 'canvas_rect', 'canvas_text', 'canvas_text_right',
)
ROM_REQUIRED = ('_start', 'link_write')


def header_complement(rom):
    return (-(sum(rom[0xA0:0xBD]) + 0x19)) & 0xFF


def check_rom_bytes(rom):
    """Return a list of validation errors for raw .gba bytes (empty list = valid)."""
    errs = []
    if len(rom) < 0xC0:
        return [f'ROM too small ({len(rom)} bytes < 192-byte GBA header)']
    if len(rom) > MAX_ROM_BYTES:
        errs.append(f'ROM size {len(rom)} exceeds 64 KB single-sector budget ({MAX_ROM_BYTES})')
    if len(rom) % 4 != 0:
        errs.append(f'ROM size {len(rom)} is not a multiple of 4 bytes')

    entry = struct.unpack_from('<I', rom, 0)[0]
    if (entry & 0xFF000000) != 0xEA000000:
        errs.append(f'entry word {entry:#010x} at 0x00 is not an ARM unconditional branch (0xEA0000xx)')
    else:
        offset = entry & 0x00FFFFFF
        if offset & 0x00800000:
            offset -= 0x01000000
        target = 8 + (offset << 2)
        if not (0xC0 <= target < len(rom)):
            errs.append(f'entry branch target {target:#x} outside code range [0xC0, {len(rom):#x})')

    if rom[0x04:0xA0] != LOGO:
        errs.append('Nintendo logo (0x04..0x9F) does not match the mandatory 156-byte GBA boot logo')
    if rom[0xA0:0xAC] != b'WGC.TX.UI\x00\x00\x00':
        errs.append(f'header title {rom[0xA0:0xAC]!r} != b"WGC.TX.UI\\x00\\x00\\x00"')
    if rom[0xAC:0xB0] != b'WGCU':
        errs.append(f'header game code {rom[0xAC:0xB0]!r} != b"WGCU"')
    if rom[0xB0:0xB2] != b'01':
        errs.append(f'header maker code {rom[0xB0:0xB2]!r} != b"01"')
    if rom[0xB2] != 0x96:
        errs.append(f'fixed byte at 0xB2 is {rom[0xB2]:#04x}, must be 0x96')
    if rom[0xB3:0xBD] != b'\x00' * 10:
        errs.append(f'header unit/device/reserved/version bytes (0xB3..0xBC) not zero: {rom[0xB3:0xBD].hex()}')

    want_chk = header_complement(rom)
    if rom[0xBD] != want_chk:
        errs.append(f'header complement check at 0xBD is {rom[0xBD]:#04x}, expected {want_chk:#04x}')
    if rom[0xBE:0xC0] != b'\x00\x00':
        errs.append(f'header reserved bytes 0xBE..0xBF not zero: {rom[0xBE:0xC0].hex()}')
    if rom[-8:] != PAD_TRAILER:
        errs.append(f'ROM trailer {rom[-8:]!r} != {PAD_TRAILER!r} (truncated or missing .pad section)')
    return errs


def check_elf(elf_path, rom_len):
    """Verify ELF section placement, symbol addresses, and RAM budgets against the built ROM."""
    errs = []
    nm = subprocess.run(['arm-none-eabi-nm', elf_path], capture_output=True, text=True, check=True)
    syms = {}
    for line in nm.stdout.splitlines():
        parts = line.split()
        if len(parts) == 3:
            syms[parts[2]] = int(parts[0], 16)

    rom_end = syms.get('__rom_end__')
    if rom_end is None:
        errs.append('missing __rom_end__ symbol in ELF')
    elif rom_end - 0x08000000 != rom_len:
        errs.append(f'__rom_end__ ({rom_end:#010x}) - 0x08000000 = {rom_end - 0x08000000} != ROM size {rom_len}')

    for name in IWRAM_REQUIRED:
        addr = syms.get(name)
        if addr is None:
            errs.append(f'missing required IWRAM symbol {name}')
        elif not (0x03000000 <= addr < 0x03008000):
            errs.append(f'symbol {name} at {addr:#010x} is not in IWRAM (0x03000000..0x03007FFF)')

    for name in ROM_REQUIRED:
        addr = syms.get(name)
        if addr is None:
            errs.append(f'missing required ROM symbol {name}')
        elif not (0x08000000 <= addr < 0x08000000 + MAX_ROM_BYTES):
            errs.append(f'symbol {name} at {addr:#010x} is not in ROM (0x08000000..)')

    iheap = syms.get('__iheap_start', 0) - 0x03000000
    if not (0 < iheap <= MAX_IWRAM_STATIC):
        errs.append(f'static IWRAM usage {iheap} bytes outside (0, {MAX_IWRAM_STATIC}]')

    eheap = syms.get('__eheap_start', 0) - 0x02000000
    if not (0 <= eheap <= MAX_EWRAM_STATIC):
        errs.append(f'static EWRAM usage {eheap} bytes outside [0, {MAX_EWRAM_STATIC}]')

    objdump = subprocess.run(['arm-none-eabi-objdump', '-h', elf_path], capture_output=True, text=True, check=True)
    lines = objdump.stdout.splitlines()
    iwram_size = 0
    for idx, line in enumerate(lines):
        fields = line.split()
        if len(fields) >= 7 and fields[0].isdigit():
            sec, size, vma = fields[1], int(fields[2], 16), int(fields[3], 16)
            flags = lines[idx + 1] if idx + 1 < len(lines) else ''
            if sec == '.iwram':
                iwram_size = size
            if 'ALLOC' in flags and sec != '.pad' and size > 0:
                in_rom = 0x08000000 <= vma and vma + size <= 0x08000000 + MAX_ROM_BYTES
                in_iwram = 0x03000000 <= vma and vma + size <= 0x03008000
                in_ewram = 0x02000000 <= vma and vma + size <= 0x02040000
                if not (in_rom or in_iwram or in_ewram):
                    errs.append(f'ALLOC section {sec} (vma={vma:#010x}, size={size:#x}) outside GBA memory regions')
    if iwram_size == 0:
        errs.append('.iwram section has size 0')
    return errs


def main(argv):
    if len(argv) < 2 or len(argv) % 2 != 0:
        print('usage: check_rom.py <rom.gba> <rom.elf> [<rom2.gba> <rom2.elf> ...]', file=sys.stderr)
        return 2
    fails = 0
    for i in range(0, len(argv), 2):
        rom_path, elf_path = argv[i], argv[i + 1]
        with open(rom_path, 'rb') as f:
            rom = f.read()
        errs = check_rom_bytes(rom) + check_elf(elf_path, len(rom))
        if errs:
            fails += 1
            print(f'FAIL  {rom_path}')
            for e in errs:
                print(f'      {e}')
        else:
            print(f'PASS  {rom_path} ({len(rom)} bytes, header+ELF OK)')
    return 1 if fails else 0


if __name__ == '__main__':
    sys.exit(main(sys.argv[1:]))
