#!/usr/bin/env python3
"""Source-level mutation testing against the REAL ARM build and the emulator tests.

    python3 tests/mutate.py [--quick] [file ...]      (default: src/link.c src/core/keyword.c)

For each mutant: copy the repo's src/ to a temp tree, apply one mutation, build the ROM with the
real toolchain (make B=<tmp>), and run the protocol + timing suite (tests/run_tests.py) for
v1.1a, plus (for src/core files) the host unit tests for both protocols. A mutant is KILLED if any
test fails, INVALID if it doesn't compile (not counted), SURVIVED otherwise.
Survivors are printed with file:line so each can be checked (killed by a new test, or annotated
as equivalent with `// mutate: equivalent <reason>` on that line, which skips it).

This complements Mull (host unit tests, LLVM IR mutations): it's the only way to show that the
emulator suite would catch a protocol or timing bug in the code that actually runs on the GBA.
No third-party code: operators are simple, deliberate source rewrites. link_write.s is excluded
(frozen; its delay loop was mutation-tested by hand, see .reviews/phase0-review).
"""
import os, re, shutil, subprocess, sys, tempfile

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
DEFAULT = ['src/link.c', 'src/core/keyword.c', 'src/core/boot.c', 'src/core/pairing.c', 'src/core/text.c']

# (name, regex, replacement). Applied one occurrence at a time.
OPERATORS = [
    ('== -> !=', r'==', '!='), ('!= -> ==', r'!=', '=='),
    ('< -> <=', r'(?<![<-])<(?![<=])', '<='), ('<= -> <', r'<=', '<'),
    ('> -> >=', r'(?<![->])>(?![>=])', '>='), ('>= -> >', r'>=', '>'),
    ('& -> |', r'(?<![&])&(?![&=])', '|'), ('| -> &', r'(?<![|])\|(?![|=])', '&'),
    ('|= -> &=', r'\|=', '&='), ('&& -> ||', r'&&', '||'), ('|| -> &&', r'\|\|', '&&'),
    ('>> -> <<', r'>>', '<<'), ('<< -> >>', r'<<', '>>'),
    ('+ -> -', r'(?<![+])\+(?![+=])', '-'), ('- -> +', r'(?<![-])-(?![->=])', '+'),
    ('++ -> --', r'\+\+', '--'),
    ('true -> false', r'\btrue\b', 'false'), ('false -> true', r'\bfalse\b', 'true'),
    ('hex const +1', r'\b0x([0-9A-Fa-f]+)\b', lambda m: hex(int(m.group(1), 16) + 1)),
    ('dec const +1', r'(?<![\w.])([1-9][0-9]*)\b(?!\.)', lambda m: str(int(m.group(1)) + 1)),
    ('drop if-body', r'(\bif \([^;{]*\)) (\w[^;{]*;)', r'\1 {}'),
]


def code_lines(text):
    """Yield (lineno, start, end) for lines that are code (skip comments, includes, blank)."""
    pos = 0
    in_block = False
    for n, line in enumerate(text.split('\n'), 1):
        start, end = pos, pos + len(line)
        pos = end + 1
        s = line.strip()
        if in_block:
            if '*/' in s: in_block = False
            continue
        if s.startswith('/*'):
            in_block = '*/' not in s
            continue
        if not s or s.startswith(('//', '#', '*')) or 'mutate: equivalent' in line:
            continue
        code_end = line.find('//')
        yield n, start, start + (code_end if code_end >= 0 else len(line))


def mutants(path):
    text = open(os.path.join(ROOT, path)).read()
    for n, a, b in code_lines(text):
        seg = text[a:b]
        for name, rx, rep in OPERATORS:
            for m in re.finditer(rx, seg):
                new = m.expand(rep) if isinstance(rep, str) else rep(m)
                mutated = text[:a + m.start()] + new + text[a + m.end():]
                yield dict(file=path, line=n, op=name, orig=seg.strip(), text=mutated)


def run_one(mt, tmp):
    src = os.path.join(tmp, 'src')
    shutil.rmtree(src, ignore_errors=True)
    shutil.copytree(os.path.join(ROOT, 'src'), src)
    open(os.path.join(tmp, mt['file']), 'w').write(mt['text'])
    b = os.path.join(tmp, 'build')
    mk = ['make', '-s', '-C', tmp, f'B={b}', f'DEPS={ROOT}/build/deps']
    r = subprocess.run(mk + [f'{b}/wgc-gba-tx-ui.gba'], capture_output=True, text=True)
    if r.returncode: return 'INVALID (build)'  # not counted: compile errors aren't test kills
    if mt['file'].startswith('src/core/'):  # pure code: the host unit tests (both protocols) count too
        if subprocess.run(mk + ['unit'], capture_output=True, text=True).returncode: return 'KILLED (unit)'
    r = subprocess.run([sys.executable, os.path.join(ROOT, 'tests/run_tests.py'), os.path.join(ROOT, 'build/emu_harness'),
                        f'{b}/wgc-gba-tx-ui.gba', 'v1.1a'], capture_output=True, text=True)
    return 'KILLED (emulator)' if r.returncode else 'SURVIVED'


def main():
    args = [a for a in sys.argv[1:] if not a.startswith('--')]
    quick = '--quick' in sys.argv
    files = args or DEFAULT
    muts = [m for f in files for m in mutants(f)]
    if quick: muts = muts[::4]
    print(f'mutate: {len(muts)} mutants in {", ".join(files)}')
    with tempfile.TemporaryDirectory() as tmp:
        for f in ('Makefile', 'ld', 'scripts', 'tools', 'tests'):
            s, d = os.path.join(ROOT, f), os.path.join(tmp, f)
            (shutil.copytree if os.path.isdir(s) else shutil.copy)(s, d)
        killed, invalid, survivors = 0, 0, []
        for i, mt in enumerate(muts, 1):
            res = run_one(mt, tmp)
            killed += res.startswith('KILLED')
            invalid += res.startswith('INVALID')
            if res == 'SURVIVED': survivors.append(mt)
            print(f"  [{i:3}/{len(muts)}] {res:15} {mt['file']}:{mt['line']:<4} {mt['op']:14} {mt['orig'][:60]}", flush=True)
    valid = len(muts) - invalid
    score = 100 * killed / valid if valid else 100
    print(f'\nmutate: {killed}/{valid} valid mutants killed ({score:.0f}%), {invalid} invalid (did not build). Survivors:')
    for mt in survivors: print(f"  {mt['file']}:{mt['line']}  {mt['op']}  {mt['orig']}")
    sys.exit(1 if survivors else 0)  # every survivor is a test gap or needs an 'equivalent' annotation


if __name__ == '__main__':
    main()
