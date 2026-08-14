# Repository guidance

## Project direction

This repository is evolving from a Mixxx-derived Tango Mode into a dedicated
tango DJ application. Prefer decisions that support tango-native workflow while
preserving a familiar deck-based DJ interface.

## Model routing guidance

Use stronger models for irreversible or architecture-shaping work.

- Design, architecture, product planning, and data-model decisions: prefer
  `gpt-5.6-sol` or `gpt-5.6-terra` with medium reasoning.
- Risky implementation touching AutoDJ, playback, database schema, transition
  behavior, or major UI model/view code: prefer `gpt-5.6-sol` or
  `gpt-5.6-terra`; do not default to `gpt-5.6-luna`.
- Mechanical implementation after a clear plan: `gpt-5.6-luna` is acceptable.
- Log reading, summaries, test triage, small refactors, and bounded searches:
  `gpt-5.6-luna` with low or medium reasoning is acceptable.
- Final review before committing behavior-sensitive changes: use `gpt-5.6-sol`
  or `gpt-5.6-terra`.
- If the active model appears underpowered for the task, pause and recommend
  switching before making broad design, playback, or database changes.

## Session role boundaries

When using named Codex profiles, keep each session within its role:

- `design`: may inspect code and create or update design documents under
  `docs/`. It must not edit source code, build files, tests, resources, or
  scripts.
- `implementation`: may edit source code, tests, resources, build files, and
  scripts needed for the accepted implementation plan. It must not update design
  documents or planning notes unless explicitly asked.
- `mechanical`: read-only. Use it for logs, searches, summaries, test-output
  analysis, and mechanical triage. It must not write files, commit, stash,
  rebase, or run mutating commands.
- `review`: read-only. Use it for final review before commit or push. It must
  not write files, commit, stash, rebase, or run mutating commands.

Only one Codex session should perform writes at a time. Before any writing
session starts work, check `git status --short` and preserve unrelated changes.

## Development expectations

- Preserve unrelated user changes and untracked notes.
- Keep commits focused and omit co-authored trailers unless explicitly asked.
- Validate playback, AutoDJ, cue, and database changes with focused tests when
  practical.
