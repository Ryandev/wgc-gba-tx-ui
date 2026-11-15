#!/usr/bin/env python3
"""Source-level mutation testing against the REAL ARM build, the unit tests and the emulator.

    python3 tests/mutate.py [--quick] [file ...]      (default: DEFAULT below; make mutate)

For each mutant: copy the repo's src/ to a temp tree, apply one mutation, build the ROM with the
real toolchain, then score it with (for src/core files) the host unit tests for both protocols,
the v1.1a protocol/timing suite (tests/run_tests.py) and the v1.1a golden images (tests/visual.py).

Outcomes
  KILLED (unit|emulator|visual|timeout)  a test noticed the mutation
  INVALID (build|host build)             the mutant doesn't compile (not counted)
  SURVIVED                               nothing noticed: a test gap (make mutate fails)
  ACCEPTED                               not run: a known, reasoned exception (reported, not a kill)
Infrastructure failures (harness missing or crashing, unexpected exceptions) ABORT the run with
exit 3. They are never counted as kills (runner re-check F3).

Annotations, on the same line as the code, scoped to named operators:
  // mutate: equivalent[op, op] reason  the mutant provably can't change behaviour (skipped)
  // mutate: accepted[op, op] reason    it CAN change behaviour, but untestable here for a stated
                                        reason (reported as ACCEPTED, never as killed)
No third-party code. link_write.s is excluded (frozen; delay loop mutation-tested by hand).
"""
import os, re, shutil, subprocess, sys, tempfile

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
DEFAULT = ['src/link.c', 'src/main.c', 'src/screen_main.c', 'src/core/keyword.h', 'src/core/keyword.c',
           'src/core/boot.c', 'src/core/pairing.c', 'src/core/text.c', 'src/core/view.c', 'src/core/lcd.c',
           'src/core/notes.c', 'src/core/splash.c',
           'src/core/settings.c', 'src/core/canvas.c', 'src/core/zoom.c', 'src/screen_settings.c']
INFRA = 3      # exit code run_tests.py / visual.py / unit-fast use for infrastructure errors
NO_BUILD = 200  # unit-fast: host compile failed

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


class InfraError(Exception):
    pass


def annotated(line, kind):
    """Operators named in '// mutate: <kind>[op, op] reason' on this line (empty set if none)."""
    m = re.search(rf'mutate: {kind}\[([^\]]*)\]', line)
    return {o.strip() for o in m.group(1).split(',')} if m else set()


def code_lines(text):
    """Yield (lineno, start, end, equivalent_ops, accepted_ops) for code lines."""
    pos, in_block = 0, False
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
        yield (n, start, start + (code_end if code_end >= 0 else len(line)),
               annotated(line, 'equivalent'), annotated(line, 'accepted'))


def mutants(path):
    text = open(os.path.join(ROOT, path)).read()
    for n, a, b, equiv, accepted in code_lines(text):
        seg = text[a:b]
        for name, rx, rep in OPERATORS:
            if name in equiv: continue
            for m in re.finditer(rx, seg):
                new = m.expand(rep) if isinstance(rep, str) else rep(m)
                yield dict(file=path, line=n, op=name, orig=seg.strip(), accepted=name in accepted,
                           text=text[:a + m.start()] + new + text[a + m.end():])


# Core modules also exercised through another module's tests (phase1-chunk4: canvas.c's glyph code is
# tested through lcd.c, so running only test_canvas.c left real kills uncounted)
ALSO_TESTED_BY = {'canvas': ['lcd']}


def unit_tests_for(path):
    """Unit tests that exercise a mutated file (all of them for a header)."""
    base = os.path.splitext(os.path.basename(path))[0]
    own = os.path.join('tests', 'unit', f'test_{base}.c')
    if path.endswith('.c') and os.path.exists(os.path.join(ROOT, own)):
        return [own] + [os.path.join('tests', 'unit', f'test_{b}.c') for b in ALSO_TESTED_BY.get(base, [])]
    return sorted(os.path.join('tests', 'unit', f) for f in os.listdir(os.path.join(ROOT, 'tests', 'unit'))
                  if f.startswith('test_') and f.endswith('.c'))


def run(cmd, timeout):
    try:
        r = subprocess.run(cmd, capture_output=True, text=True, timeout=timeout)
        return r.returncode, r.stdout[-2000:] + r.stderr[-2000:]
    except subprocess.TimeoutExpired:
        return 124, 'timeout'


def evaluate(tmp, files_under_test):
    """Build + test whatever is in tmp/src. -> outcome string. Raises InfraError."""
    b = os.path.join(tmp, 'build')
    # Fresh outputs every time: copytree keeps old mtimes, so make could reuse a previous mutant's
    # objects (chunk2 F2); stale visual output must not leak either (re-check F5).
    for d in ('obj', 'unit-fast'):
        shutil.rmtree(os.path.join(b, d), ignore_errors=True)
    shutil.rmtree(os.path.join(tmp, 'visual'), ignore_errors=True)
    for f in ('wgc-gba-tx-ui.elf', 'wgc-gba-tx-ui.gba'):
        if os.path.exists(os.path.join(b, f)): os.remove(os.path.join(b, f))
    mk = ['make', '-s', '-C', tmp, f'B={b}', f'DEPS={ROOT}/build/deps']
    if run(mk + [f'{b}/wgc-gba-tx-ui.gba'], 300)[0]: return 'INVALID (build)'
    units = sorted({t for f in files_under_test if f.startswith('src/core/') for t in unit_tests_for(f)})
    if units:
        result = os.path.join(b, 'unit-fast', 'result')
        rc, out = run(mk + ['unit-fast', 'UNIT_TESTS=' + ' '.join(units)], 300)
        if rc == 124: return 'KILLED (timeout)'
        if not os.path.exists(result): raise InfraError(f'unit-fast wrote no result file:\n{out}')
        rc = int(open(result).read().strip())  # make's own exit code is useless here (see Makefile)
        if rc == NO_BUILD: return 'INVALID (host build)'
        if rc == 124: return 'KILLED (timeout)'
        if rc == 1: return 'KILLED (unit)'
        if rc: raise InfraError(f'unit-fast result {rc}:\n{out}')
    rom, harness = f'{b}/wgc-gba-tx-ui.gba', os.path.join(ROOT, 'build/emu_harness')
    for stage, cmd in (('emulator', ['tests/run_tests.py', harness, rom, 'v1.1a']),
                       ('visual', ['tests/visual.py', harness, rom, 'v1.1a', f'--out={tmp}/visual'])):
        rc, out = run([sys.executable, os.path.join(ROOT, cmd[0])] + cmd[1:], 300)
        if rc == 124: return 'KILLED (timeout)'
        if rc == 1: return f'KILLED ({stage})'
        if rc: raise InfraError(f'{stage} stage exit {rc}:\n{out}')
    return 'SURVIVED'


def main():
    args = [a for a in sys.argv[1:] if not a.startswith('--')]
    quick = '--quick' in sys.argv
    files = args or DEFAULT
    all_muts = [m for f in files for m in mutants(f)]
    accepted = [m for m in all_muts if m['accepted']]
    muts = [m for m in all_muts if not m['accepted']]
    if quick: muts = muts[::4]
    print(f'mutate: {len(muts)} mutants (+{len(accepted)} accepted, not run) in {", ".join(files)}')
    if subprocess.run(['make', '-s', '-C', ROOT, 'build/emu_harness'], capture_output=True).returncode:
        sys.exit('mutate: cannot build build/emu_harness')
    with tempfile.TemporaryDirectory() as tmp:
        for f in ('Makefile', 'assets', 'ld', 'scripts', 'tools', 'tests'):
            s, d = os.path.join(ROOT, f), os.path.join(tmp, f)
            (shutil.copytree if os.path.isdir(s) else shutil.copy)(s, d)
        try:
            # Baseline: the UNMUTATED tree must pass every stage, or every "kill" would be fake.
            shutil.copytree(os.path.join(ROOT, 'src'), os.path.join(tmp, 'src'))
            base = evaluate(tmp, files)
            if base != 'SURVIVED':
                sys.exit(f'mutate: ABORT - unmutated baseline did not pass ({base}); fix that first')
            killed, invalid, survivors = 0, 0, []
            for i, mt in enumerate(muts, 1):
                shutil.rmtree(os.path.join(tmp, 'src'))
                shutil.copytree(os.path.join(ROOT, 'src'), os.path.join(tmp, 'src'))
                open(os.path.join(tmp, mt['file']), 'w').write(mt['text'])
                res = evaluate(tmp, [mt['file']])
                killed += res.startswith('KILLED')
                invalid += res.startswith('INVALID')
                if res == 'SURVIVED': survivors.append(mt)
                print(f"  [{i:3}/{len(muts)}] {res:20} {mt['file']}:{mt['line']:<4} {mt['op']:14} {mt['orig'][:56]}",
                      flush=True)
        except InfraError as e:
            print(f'mutate: ABORT - infrastructure error (not a test result):\n{e}')
            sys.exit(INFRA)
    valid = len(muts) - invalid
    score = 100 * killed / valid if valid else 100
    print(f'\nmutate: {killed}/{valid} valid mutants killed ({score:.0f}%), {invalid} invalid (did not build), '
          f'{len(accepted)} accepted (not run).')
    for mt in accepted: print(f"  ACCEPTED {mt['file']}:{mt['line']}  {mt['op']}  {mt['orig']}")
    print('Survivors:' if survivors else 'Survivors: none')
    for mt in survivors: print(f"  {mt['file']}:{mt['line']}  {mt['op']}  {mt['orig']}")
    sys.exit(1 if survivors else 0)  # every survivor is a test gap or needs a reasoned annotation


if __name__ == '__main__':
    main()
