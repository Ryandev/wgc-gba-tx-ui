---
name: gemini-review
description: Adversarial review of this project's work by Gemini (agy CLI), read-only. Use when the owner asks for Gemini's feedback or a second opinion, at the end of a phase, or before a design decision is locked in. Claude writes a brief that asks Gemini to attack our claims, fires it with scripts/gemini_review.sh, then verifies every finding against the code before reporting. Gemini's reply is a claim, not a verdict.
---

# Adversarial review with Gemini (agy)

Pattern taken from `../CAD/.claude/skills/delegate` and `../CAD/lib/round.sh`, adapted for
**review, not implementation**. There, agy writes the code and Claude judges. Here, agy *only
critiques*: it runs in `--mode plan`, can't edit, and the script rejects the run if any tracked
file changed.

The reason: Claude designed most of this project. A reviewer that hasn't been anchored on
Claude's reasoning is the only independent check before hardware. So the brief must invite
disagreement and hand over **the code and claims, not Claude's conclusions about them**.

## 1. Write the brief

Into `.reviews/briefs/<tag>.md` (git-ignored). A brief that works has:

1. **Context in 5 lines:** what the project is and what stage it's at. Point at files, don't
   paste them. Gemini reads the repo itself.
2. **Scope:** the exact files or dirs to review, and what's out of scope.
3. **The claims under attack**, numbered, each with where it's made (file + section). Make them
   falsifiable, e.g. "C3: `link_write.s` reproduces the stock ROM's bus pattern (HARDWARE.md §4,
   src/link_write.s)". This is the heart of the brief.
4. **The adversarial instruction**, verbatim:
   > Your job is to find what is wrong, unproven, risky or missing, not to agree. Assume the
   > author is overconfident. For every claim, either break it with evidence (file:line, a
   > datasheet or source URL, or a concrete failure scenario) or say plainly that you couldn't.
   > Agreement without evidence is worthless to us; "looks good" is not a finding. Distinguish
   > fact from opinion. If you think a whole approach is wrong, say so and propose the
   > alternative. You may not edit files.
5. **Owner decisions that bind** (from SPEC.md "Decided" notes and the conversation). Gemini
   may argue against them, but must label that "challenges an owner decision" so it reaches the
   owner as a question, not as a silent change.
6. **Output:** it must match `.claude/skills/gemini-review/findings.schema.json` (the script
   passes it with `--json-schema`).

## 2. Fire it

    bash scripts/gemini_review.sh .reviews/briefs/<tag>.md

Run it with `run_in_background: true`. It takes minutes, and the harness re-invokes Claude when
it exits. It writes `.reviews/<tag>/` with `reply.json`, `findings.md` and `log.md`, uses a fresh
agy conversation per tag, and fails if the working tree changed.
Env: `GEMINI_REVIEW_MODEL` (default `gemini-4-argon-high`), `GEMINI_REVIEW_TIMEOUT` (default `40m`).

## 3. Verify every finding (don't relay it raw)

For each finding, Claude checks the evidence itself (reads the cited lines, re-runs `make test`,
re-derives the number) and gives it one verdict in `.reviews/<tag>/verdict.md`:

| Verdict | Meaning |
|---|---|
| **CONFIRMED** | Checked and true. Becomes a fix or a doc correction. |
| **REJECTED** | Checked and false. Say why, with the counter-evidence. |
| **NEEDS HARDWARE** | Can't be settled in the emulator. Add it to the hardware test list. |
| **OWNER CALL** | An opinion, or a challenge to an owner decision. Goes to the owner as a question. |

Watch for: invented file:line references, "best practice" claims with no source, findings about
code that doesn't exist, and confident numbers that come from nowhere. Gemini is a reviewer
with the same failure modes as Claude.

## 4. Report

Tell the owner: how many findings, the CONFIRMED ones (most severe first) with the fix proposed,
the OWNER CALL questions, and the NEEDS HARDWARE additions. Don't apply design-level changes
without the owner. Doc typos and clear bugs can be fixed, re-tested and committed.
