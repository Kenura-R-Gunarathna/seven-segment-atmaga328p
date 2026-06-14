# Discussions / decision log

Design questions worked out during development, and the decisions that came from them — so the
*why* behind a choice isn't lost. Keeps the hardware/design docs clean (those state the final
answer; this is the reasoning trail).

## How it's maintained
Run **`/qna`** (see `.claude/skills/qna/`) and it appends the current question + answer/decision
to a dated file here. Entries are append-only; one file per day, newest entry at the bottom.

## File / entry format
- File: `YYYY-MM-DD-<slug>.md` (e.g. `2026-06-12-power-supply.md`).
- Entry:
  ```
  ## <HH:MM> — <short title>
  **Q:** <the question>
  **A / decision:** <the answer or what was decided, concise>
  **Why:** <key reasoning, if not obvious>
  ```

## Index
| Date | Topic |
|------|-------|
| 2026-06-12 | [Power supply — burn cause + buck/linear redesign](2026-06-12-power-supply.md) |
| 2026-06-13 | [Power supply (final) + PCB layout — neg-rail/inverting-buck, separate PCB, layout](2026-06-13-power-and-pcb.md) |
