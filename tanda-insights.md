# Tanda Insights — Set-Awareness & Freshness Assistant

> Working title. Originally conceived as the "Adaptive Tanda Suggestion Engine."
> The name was deliberately narrowed during design — see [Framing](#framing-mirror-not-oracle).

Status: **Design / not started**
Target: a release later in 2026
Owner: Seemanta (Tango DJ build)

---

## 1. One-line summary

An **optional info pane** that reflects a DJ's own play history back at them —
"have I overplayed this, when did I last play it *here*, is this fresh for this
room" — so they can keep their sets fresh and spot their own blind spots. It
**does not** predict crowd response or tell the DJ what to play.

---

## 2. Framing: mirror, not oracle

This feature went through an important pivot during design. It is worth
recording *why*, because the framing constrains every downstream decision.

**Rejected framing — prediction.** The original idea included AI/ML that would
suggest tandas likely to please the crowd, trained on played sets plus a 0–10
"how did the set go" score.

Why it was rejected:

- **Data scale is fatal.** One DJ plays ~12–18 tandas per milonga, ~30–60
  events/year → ~600–900 tandas/year. Even after 5 years that's only a few
  thousand examples, spread across cities, seasons, rooms and crowds that each
  change the "right answer." That is far too small and too noisy for ML to
  converge into anything trustworthy. This is a permanent cold-start problem for
  a single-user tool; it only ever warms up with *pooled* multi-DJ data, which
  is a completely different product (cloud service, privacy, network effects).
- **The labels are worthless.** Self-reported, post-hoc set scores by the person
  who chose the music are low-variance and self-serving. A model learns from
  *contrast*; if every set scores 8–10, there is no signal. (Real anecdote: DJ
  once tracked tandas on paper — they were all bangers. Lovely, and exactly why
  the label signal is too weak to learn from.)
- **DJs reject oracles.** "This tool knows what the floor wants better than you"
  is rejected on principle and ignored the moment it's wrong in front of a live
  floor. DJs override on instinct.

**Accepted framing — reflection.** Instead of predicting the future, the tool
*reflects the past*: it surfaces patterns in the DJ's own history and lets the
DJ decide what to do about them. This:

- needs only **the DJ's own history + simple statistics** (`GROUP BY`, `COUNT`) —
  no ML, no cold-start, no crowd-response labels;
- is **explainable and never embarrassingly wrong** — every insight is a fact
  the DJ can verify;
- is an **honest pitch**: "this tool tells you when you're on autopilot" is
  something an experienced DJ *believes*, because they know they have blind spots.

> **Design rule that follows from this:** every insight is phrased as a *neutral
> fact*, never a verdict. Show "4 of your last 6 milongas," never "Overplayed!".
> The number carries the meaning; the DJ judges whether that's their signature
> closer or a rut. **Signature vs. staleness is always the DJ's call, not the
> tool's.**

---

## 3. What it shows: "This tanda at a glance…"

A **passive, toggleable pane** the DJ turns on if they want it. Pull, not push:
the tool never interrupts, flags, or nags — it just *has the answer ready* when
the DJ turns to look. Insights are scoped to **the tanda/track currently in
focus** (selected / cued / hovered), not a running feed about the whole set.

Heading: **"This tanda at a glance…"**

Target content — a *glanceable handful* only (dies the moment it's a wall of
stats). Roughly five lines a DJ can absorb in under two seconds:

- **Last played:** e.g. "3 weeks ago, here in Berlin" / "never here" / "never"
- **Recent frequency:** e.g. "4 of your last 6 milongas"
- **This room:** e.g. "Played here 2× this year" vs. "Fresh for this room"
- **In tonight's set:** "Not yet played tonight" / "⚠ you queued a Di Sarli
  tanda 20 min ago"
- **Rotation note (optional):** e.g. "Your 3rd D'Arienzo tonight"

Everything else (full history, charts) lives behind a **"details" expand**, not
on the glance surface.

### Copy & interaction rules

- **Neutral tone.** Facts, not verdicts (see design rule above).
- **Empty state is most of the experience in month one.** Most tandas will show
  "First time playing this" / "No history yet." That must read as *informative*
  ("this is new for you"), not *broken* ("empty pane"). Design it deliberately.
- **Let DJs mute a pattern** as "intentional / signature" so it stops being
  surfaced.
- **Don't oversell "fresh."** Freshness for the *dancers* (who mostly don't
  track orquestas consciously) differs from freshness for *you* (bored of a
  tanda long before they are). We solve the DJ's sense of repetition and blind
  spots — real and worth solving — not a claim that the floor notices.

---

## 4. The candidate insights (all just statistics)

Each of these is a `GROUP BY` / `COUNT` over play history — fast, offline,
debuggable, never wrong:

| Insight | Definition | Notes |
|---|---|---|
| **Overplay detection** | count of a track/tanda across recent sessions | **Must be scoped by venue/crowd**, not global. A touring DJ replaying a favorite every week to a *different* crowd is fine; only a *residency* makes it stale. Global-only overplay warnings make the tool useless for touring DJs. |
| **Rotation health** | share of tandas by orquesta/style over a window | e.g. "70% of your rhythmic tandas this month were D'Arienzo" → library imbalance. |
| **Pattern / staleness flags** | recurring sequences | e.g. "opened with the same tanda 3 gigs running," "you almost always follow Pugliese with a milonga." Surface, don't judge — could be signature. |
| **Per-room freshness** | overlap of tonight's plan with last time in this city/venue | Requires gig context (§6). |

---

## 5. Codebase findings (as of this branch)

Read-only investigation of the Mixxx fork. Tech stack per layer:

| Layer | Technology | Key files |
|---|---|---|
| Core logic | C++17, Qt Core | `src/library/autodj/autodjprocessor.cpp/.h` |
| Config storage | QSettings (`settings` table, `[AutoDJ]` section) | keep_queue, cortina_length, etc. |
| UI widgets | Qt **Widgets** (not QML) | `src/library/autodj/dlgautodj.ui`, `dlgautodj.cpp` |
| Dockable panel | QDockWidget + custom title bar | `src/library/autodj/autodjfeature.cpp`, `src/widget/wdocktitlebar.cpp/.h` |
| Data model | Qt MVC (`PlaylistTableModel` → `WTrackTableView`) | `src/library/playlisttablemodel.cpp`, `src/widget/wtracktableview.cpp` |
| Theming | Qt Style Sheets (`.qss`) | `res/skins/LateNight/style_*.qss` |
| Session cortina tags | in-memory `QSet<TrackId>` | `src/library/autodj/cortinaregistry.cpp/.h` |
| Database | SQLite | `res/schema.xml`, `src/library/dao/trackschema.h` |

### 5a. Good news — the pane has a proven home

The fork **already built the exact UI pattern the pane needs**: the Auto DJ
Queue dock is a toggleable, dockable, position-persisting `QDockWidget` with a
custom title bar, gated behind `[AutoDJ],keep_queue`, toggled from the View
menu.

- Entry point: `AutoDJFeature::createAutoDJDockWidget()` in
  `src/library/autodj/autodjfeature.cpp` (~line 164).
- Visibility gated by Tango mode control `[AutoDJ],keep_queue`.
- Size/position persisted via `QMainWindow::saveState()/restoreState()`.
- Because it's a `QDockWidget`, it can float / move to a **second monitor** —
  solves the "main screen already full of waveforms" real-estate concern.

→ The "at a glance" pane is a **parallel copy of infrastructure that already
works**: a second dock widget, same gating, same save/restore, same styling
hook (`AutoDJFeature::libraryStyleSheet()`). The UI shell is nearly free.

### 5b. Hard truth — the data the feature needs does NOT exist

This is the finding that dominates scope. Mixxx records **almost no play
history**:

- Per track, the DB stores only an **aggregate `timesplayed` count** and a
  **single `last_played_at` timestamp** (`res/schema.xml`, `PlayCounter` in
  `src/track/playcounter.h`). That's it.
- **No per-play log.** "4 of your last 6 milongas" is *uncomputable* today — you
  cannot reconstruct history from a running total and one timestamp.
- **No venue / city / session** concept anywhere in the schema. The entire
  "fresh for *this room*" idea has zero backing data.
- **No tanda grouping**, even in memory. Cortina tags are session-only
  (`CortinaRegistry`, cleared on restart) and never persisted. The keep-queue
  cursor (`m_keepQueueRow`) is memory-only too.

→ **The mirror is a data-plumbing project, not a UI project.** The pane is the
easy 20%; the history log underneath is the real 80%.

> ⚠️ Note for later: an earlier exploration pass conflated "at a glance" with a
> *live queue summary* (current tanda's tracks, cortina countdown, BPM range).
> That is a **separate, smaller feature** that needs no new data and shares only
> the name and the dock. Do not confuse it with the freshness mirror below.

---

## 6. What must be built (in dependency order)

1. **Persistent play-history table** — the keystone. One row per play:
   `(track_id, played_at, venue/city, session_id)`. New migration in
   `res/schema.xml`. Written when a track goes live from the AutoDJ processor.
2. **Gig context** — a lightweight venue/city the DJ sets at session start (text
   field, remembered per session). Needed to scope overplay to a *room* instead
   of globally. Without this, overplay warnings are useless to touring DJs.
3. **Persisted tanda grouping** — even a simple "cortina-to-cortina = one tanda"
   derivation, so history is queryable at the tanda level, not just track level.
4. **The pane** — read-only view: cheap `GROUP BY`/`COUNT` queries against the
   table, shown for the focused track/tanda. Parallel to
   `createAutoDJDockWidget()`.

Suggested new files (parallel to existing patterns):

- `src/library/autodj/playhistorydao.{cpp,h}` — writes/queries the history table.
- `src/library/autodj/tandaglancepanel.{cpp,h}` — the QWidget pane content.
- `AutoDJFeature::createTandaGlanceDockWidget()` — parallel to the queue dock.
- Migration bump in `res/schema.xml`.
- Write hook in `autodjprocessor.cpp` where a track transitions to now-playing.

---

## 7. Product sequencing — value is backloaded

Steps 1–3 are **invisible plumbing that produces no visible feature** until
they've been running for several gigs to accumulate data. Consequence:

- **Ship the logging quietly and early** — ideally folded into the 2026 release
  already planned — so that by the time the pane ships there is history to
  reflect.
- A pane released the *same day* as the logging shows nothing but "first time
  playing this" for weeks.
- Optionally expose the raw history as **CSV/JSON export** early, so the DJ (and
  early adopters) can eyeball the data before the pane exists.

**Recommended split:**

- **Now / 2026 release:** play-history log + gig context (steps 1–2, maybe 3).
  Silent, foundational. Optional CSV/JSON export.
- **Next:** the "at a glance" pane (step 4) as a read-only view over accumulated
  history.

---

## 8. Explicit non-goals

- ❌ Predicting crowd response or set success.
- ❌ Telling the DJ what to play next.
- ❌ Any ML / model training at single-DJ scale.
- ❌ Nagging, blocking, or interrupting the DJ during a set.
- ❌ Claiming the *dancers* notice freshness (we serve the *DJ's* sense of it).

## 9. Open questions

- How is gig context (venue/city) entered — free text, dropdown of past venues,
  or geolocation? Free text is simplest for v1.
- Tanda grouping: auto-derive from cortinas only, or also allow manual tanda
  markers?
- Retention / privacy: history is local SQLite; is any export/sync ever wanted?
  (Relevant if a future pooled-data product is ever considered — see §2.)
- Where exactly does the pane read "focused" tanda from — cue, selection, or
  hover in the queue table?
