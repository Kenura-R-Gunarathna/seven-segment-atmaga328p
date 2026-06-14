---
name: qna
description: Log the current design question + answer/decision to docs/discussions/. Use when the user types /qna or says "log this", "record this decision", "save this Q&A" — typically right after a design question has been worked out in the conversation.
---

# /qna — append a design Q&A / decision to the discussion log

Capture the **most recent design question and its resolution** from this conversation into the
project's discussion log so the reasoning isn't lost. The hardware/design docs hold the final
answer; this log holds the *why*.

## Steps
1. Determine today's date (the harness provides `currentDate`) → `YYYY-MM-DD`. If the user names a
   different date, use that.
2. Pick the target file `docs/discussions/<YYYY-MM-DD>-<slug>.md`:
   - `<slug>` = 2–4 kebab-case words for the topic (e.g. `power-supply`, `sar-resolution`).
   - If a file for that date+topic already exists, **append** to it; otherwise create it with an
     `# <date> — <Topic>` H1.
3. Append one entry, newest at the bottom, in this exact format:
   ```
   ## <HH:MM> — <short title>
   **Q:** <the question, condensed>
   **A / decision:** <the answer or what was decided — concise, the actionable conclusion>
   **Why:** <key reasoning, only if not obvious>
   ```
   - Use the conversation's real content; condense, don't transcribe. Multiple related Q&As from
     the same exchange may be separate `##` entries.
   - If a number/value/part was decided, state it explicitly.
4. If the date file is new, add it to the table in `docs/discussions/README.md` (`| date | [title](file) |`).
5. Confirm to the user: which file, which entry title(s) appended.

## Rules
- **Append-only** — never rewrite or delete existing entries.
- Keep it tight (a few lines per entry). This is a decision trail, not a transcript.
- Only logs to `docs/discussions/` — don't touch other files.
- If there's no clear recent Q&A to log, ask the user what to record rather than inventing one.
