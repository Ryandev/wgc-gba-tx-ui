#!/usr/bin/env python3
"""Source-level mutation testing against the REAL ARM build and the emulator tests.

    python3 tests/mutate.py [--quick] [file ...]      (default: src/link.c src/core/keyword.c)

For each mutant: copy the repo's src/ to a temp tree, apply one mutation, build the ROM with the
real toolchain (make B=<tmp>), and run the protocol + timing suite (tests/run_tests.py) for
v1.1a, the golden-image tests (v1.1a), plus (for src/core files) the host unit tests for both protocols. A mutant is KILLED if any
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
DEFAULT = ['src/link.c', 'src/main.c', 'src/core/keyword.h', 'src/core/keyword.c', 'src/core/boot.c', 'src/core/pairing.c', 'src/core/text.c']

# (name, regex, replacement). Applied one occurrence at a time.
OPERATORS = [
    ('== -> !=', r'==', '!='), ('!= -> ==', r'!=', '=='),
    ('< -> <=', r'(?<![<-])<(?![<=])', '<='), ('<= -> <', r'<=', '<'),
    ('> -> >=', r'(?<![->])>(?![>=])', '>='), ('>= -> >', r'>=', '>'),
    ('& -> |', r'(?<![&])&(?![&=])', '|'), ('| -> &', r'(?<![|])\|(?![|=])', '&'),
    ('|= -> &=', r'\|=', '&='), ('&& -> ||', r'&&', '||'), ('|| -> &&', r'\|\|', '&&'),
    ('>> -> <<', r'>>', '<<'), ('<< -> >>', r'<<', '>>'),
    ('+ -> -', r'(?<![+])\+(?![+=])', '-'), ('- -> +', r'(?<![-])-(?![->=])', '+'),
    ('++ -> --', r'\+\+', '--'), ('-- -> ++', r'--', '++'),
    ('0 -> 1', r'(?<![\w.])0(?![\w.])', '1'),
    ('true -> false', r'\btrue\b', 'false'), ('false -> true', r'\bfalse\b', 'true'),
    ('hex const +1', r'\b0x([0-9A-Fa-f]+)\b', lambda m: hex(int(m.group(1), 16) + 1)),
    ('dec const +1', r'(?<![\w.])([1-9][0-9]*)\b(?!\.)', lambda m: str(int(m.group(1)) + 1)),
    ('drop if-body', r'(\bif \([^;{]*\)) (\w[^;{]*;)', r'\1 {}'),
]


def skipped_ops(line):
    """'// mutate: equivalent[op, op] reason' skips only those operators on this line (chunk2 F4:
    a bare whole-line annotation hid real mutants). Returns a set of names; empty = none skipped."""
    m = re.search(r'mutate: equivalent\[([^\]]*)\]', line)
    return {o.strip() for o in m.group(1).split(',')} if m else set()


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
        if not s or s.startswith(('//', '#', '*')):
            continue
        code_end = line.find('//')
        yield n, start, start + (code_end if code_end >= 0 else len(line)), skipped_ops(line)


def mutants(path):
    text = open(os.path.join(ROOT, path)).read()
    for n, a, b, skip in code_lines(text):
        seg = text[a:b]
        for name, rx, rep in OPERATORS:
            if name in skip: continue
            for m in re.finditer(rx, seg):
                new = m.expand(rep) if isinstance(rep, str) else rep(m)
                mutated = text[:a + m.start()] + new + text[a + m.end():]
                yield dict(file=path, line=n, op=name, orig=seg.strip(), text=mutated)


def unit_tests_for(path):
    """Unit tests that exercise a mutated file (all of them for a header)."""
    base = os.path.splitext(os.path.basename(path))[0]
    own = os.path.join('tests', 'unit', f'test_{base}.c')
    if path.endswith('.c') and os.path.exists(os.path.join(ROOT, own)): return [own]
    return sorted(os.path.join('tests', 'unit', f) for f in os.listdir(os.path.join(ROOT, 'tests', 'unit'))
                  if f.startswith('test_') and f.endswith('.c'))


def run(cmd, timeout):
    try:
        return subprocess.run(cmd, capture_output=True, text=True, timeout=timeout).returncode
    except subprocess.TimeoutExpired:
        return 124


def run_one(mt, tmp):
    src = os.path.join(tmp, 'src')
    shutil.rmtree(src, ignore_errors=True)
    shutil.copytree(os.path.join(ROOT, 'src'), src)
    open(os.path.join(tmp, mt['file']), 'w').write(mt['text'])
    b = os.path.join(tmp, 'build')
    # Fresh objects every mutant: copytree keeps old mtimes, so make could otherwise reuse a previous
    # mutant's .o (chunk2 review F2). libtonc/crt0/gbafix are kept: they never change.
    for d in ('obj', 'unit-fast'):
        shutil.rmtree(os.path.join(b, d), ignore_errors=True)
    for f in ('wgc-gba-tx-ui.elf', 'wgc-gba-tx-ui.gba'):
        if os.path.exists(os.path.join(b, f)): os.remove(os.path.join(b, f))
    mk = ['make', '-s', '-C', tmp, f'B={b}', f'DEPS={ROOT}/build/deps']
    if run(mk + [f'{b}/wgc-gba-tx-ui.gba'], 300): return 'INVALID (build)'   # compile errors aren't kills
    if mt['file'].startswith('src/core/'):
        result = os.path.join(b, 'unit-fast', 'result')
        if os.path.exists(result): os.remove(result)
        if run(mk + ['unit-fast', 'UNIT_TESTS=' + ' '.join(unit_tests_for(mt['file']))], 300) == 124:
            return 'KILLED (timeout)'
        # make's own exit code can't distinguish a failing test from a compile error: read the result
        rc = int(open(result).read().strip()) if os.path.exists(result) else 2
        if rc == 2: return 'INVALID (host build)'
        if rc == 124: return 'KILLED (timeout)'
        if rc: return 'KILLED (unit)'
    rom, harness = f'{b}/wgc-gba-tx-ui.gba', os.path.join(ROOT, 'build/emu_harness')
    rc = run([sys.executable, os.path.join(ROOT, 'tests/run_tests.py'), harness, rom, 'v1.1a'], 300)
    if rc: return 'KILLED (timeout)' if rc == 124 else 'KILLED (emulator)'
    rc = run([sys.executable, os.path.join(ROOT, 'tests/visual.py'), harness, rom, 'v1.1a', f'--out={tmp}/visual'], 300)
    if rc: return 'KILLED (timeout)' if rc == 124 else 'KILLED (visual)'
    return 'SURVIVED'


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
