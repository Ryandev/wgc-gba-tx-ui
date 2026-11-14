#!/usr/bin/env python3
"""Record the stock insideGadgets ROMs' cart-bus behaviour as test baselines.

    python3 tests/record_baselines.py <emu_harness> <stock_v1.1a.gba> <stock_v1.1b.gba>

Writes tests/baselines/<ver>/<scenario>.log (our own recordings; the ROMs themselves are not in the
repo, see FLASHING.md for where to get them and reference/stock-rom/SHA256SUMS to verify them).
"""
import os, shutil, subprocess, sys, tempfile
from scenarios import SCENARIOS

HERE = os.path.dirname(os.path.abspath(__file__))


def main():
    harness, roms = sys.argv[1], {'v1.1a': sys.argv[2], 'v1.1b': sys.argv[3]}
    for ver, rom in roms.items():
        out = os.path.join(HERE, 'baselines', ver)
        os.makedirs(out, exist_ok=True)
        for name, sc in SCENARIOS.items():
            with tempfile.TemporaryDirectory() as d:
                subprocess.run([harness, rom, d] + sc.get('stock', sc['ours']), check=True, capture_output=True)
                shutil.copy(os.path.join(d, 'cart_writes.log'), os.path.join(out, name + '.log'))
        print(f'recorded {len(SCENARIOS)} scenarios for {ver} from {os.path.basename(rom)} -> {out}')


if __name__ == '__main__':
    main()
