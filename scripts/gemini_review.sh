#!/usr/bin/env bash
# Fire one adversarial review brief at Gemini via agy, read-only. See .claude/skills/gemini-review.
#
#   scripts/gemini_review.sh .reviews/briefs/<tag>.md
#
# Writes .reviews/<tag>/{reply.json,findings.md,log.md,conversation}. Fresh agy conversation per tag.
# --mode plan: agy may read and reason but not edit. --dangerously-skip-permissions is still
# required in print mode (the CAD project found print mode otherwise stalls or no-ops on
# permission prompts); plan mode is what keeps it read-only, and we verify that afterwards.
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$ROOT"
BRIEF="${1:?usage: scripts/gemini_review.sh <brief.md>}"
BRIEF="$(realpath --relative-to="$ROOT" "$BRIEF")"
[ -f "$BRIEF" ] || { echo "no such brief: $BRIEF"; exit 2; }
TAG="$(basename "$BRIEF" .md)"
OUT="$ROOT/.reviews/$TAG"
SCHEMA="$ROOT/.claude/skills/gemini-review/findings.schema.json"
MODEL="${GEMINI_REVIEW_MODEL:-gemini-4-argon-high}"
TIMEOUT="${GEMINI_REVIEW_TIMEOUT:-40m}"
mkdir -p "$OUT"

# Review a snapshot, not the live tree: a git worktree of HEAD, so Claude (or the owner) can keep
# working while the review runs, and any edit Gemini makes can't touch real files. Git-ignored
# local material (reference/, mockups/, build/) is symlinked in so Gemini can still read it.
SNAP="$ROOT/.reviews/.snapshot-$TAG"
git worktree remove --force "$SNAP" 2>/dev/null || true
git worktree add --detach -q "$SNAP" HEAD
for d in reference mockups build; do [ -e "$ROOT/$d" ] && ln -s "$ROOT/$d" "$SNAP/$d"; done
echo "review: snapshot of $(git rev-parse --short HEAD) at $SNAP"

set +e
(cd "$SNAP" && command agy --model "$MODEL" --mode plan --dangerously-skip-permissions \
    --output-format json --json-schema "$SCHEMA" --print-timeout "$TIMEOUT" \
    -p "$(cat "$ROOT/$BRIEF")") > "$OUT/reply.json" 2> "$OUT/stderr.txt"
rc=$?
set -e

changed="$(git -C "$SNAP" status --porcelain --untracked-files=no)"
git worktree remove --force "$SNAP"
if [ -n "$changed" ]; then
    echo "review: WARNING - Gemini modified files in the snapshot (discarded; real tree untouched):"
    echo "$changed"
fi

python3 - "$OUT/reply.json" "$OUT/findings.md" "$OUT/conversation" <<'EOF'
import json, sys
raw = open(sys.argv[1], errors="replace").read()
out = open(sys.argv[2], "w")
try:
    d = json.loads(raw[raw.index("{"):])
except ValueError:
    out.write("# reply (unparsed)\n\n" + raw); sys.exit(0)
if d.get("conversation_id"): open(sys.argv[3], "w").write(d["conversation_id"] + "\n")
resp = d.get("structured_output") or d.get("response", d)
if isinstance(resp, str):
    try: resp = json.loads(resp[resp.index('{"'):resp.rindex("}") + 1])  # skip progress chatter before the JSON
    except ValueError: out.write("# reply (text)\n\n" + resp); sys.exit(0)
out.write(f"# Gemini review\n\nstatus: {d.get('status')}\n\n## Summary\n\n{resp.get('summary','')}\n\n## Findings\n\n")
order = {"critical": 0, "high": 1, "medium": 2, "low": 3}
for f in sorted(resp.get("findings", []), key=lambda f: order.get(f.get("severity"), 9)):
    out.write(f"### {f.get('id')} [{f.get('severity')}/{f.get('kind')}] {f.get('target')}  (claim {f.get('claim','-')}, confidence {f.get('confidence')})\n\n"
              f"{f.get('finding')}\n\n**Evidence:** {f.get('evidence')}\n\n**Suggestion:** {f.get('suggestion')}\n\n")
nb = resp.get("claims_not_broken") or []
if nb: out.write("## Claims Gemini could not break\n\n" + "".join(f"- {c}\n" for c in nb))
EOF
{
    echo "## $TAG  $(date '+%Y-%m-%d %H:%M')  agy exit $rc  model $MODEL"
    echo; echo "### brief"; echo; cat "$BRIEF"; echo; echo "### findings"; echo; cat "$OUT/findings.md"; echo
} >> "$OUT/log.md"

echo "review: agy exit=$rc"
head -c 3000 "$OUT/findings.md"; echo
echo "review: full findings in $OUT/findings.md - now VERIFY each one (skill step 3)."
exit $rc
