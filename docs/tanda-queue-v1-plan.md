# Tanda queue v1 plan

## Goal

Make a tanda an optional, named grouping of consecutive tracks already in the
Tango Auto DJ queue. A tanda is a queue concept, not a playlist and not a
change to playback order.

The Auto DJ playlist remains the sole playback source. Tanda grouping changes
only how the Tango queue is displayed and manipulated.

## Agreed DJ workflow

1. Add tracks to Auto DJ normally from the library, a playlist, or history.
2. Select one or more consecutive queue tracks.
3. Use one of:
   - **Make Tango Tanda**
   - **Make Vals Tanda**
   - **Make Milonga Tanda**
   - **Make Nuevo / Alternative Tanda**
4. The selected tracks become an expandable queue group. Ungrouped tracks
   remain ordinary individual rows.
5. A tanda can be moved as one intact block up or down the queue.

Classification is deliberately explicit. It never infers intent from track
metadata or proximity.

## Behaviour contract

### Structure

- A tanda owns one non-empty, consecutive, non-overlapping range of Auto DJ
  queue rows.
- Tanda type is descriptive in v1. It must not change Auto DJ transition or
  playback behaviour.
- Collapsing a tanda hides its child tracks. It does not remove, skip, reorder,
  or otherwise change them.
- A collapsed header shows the type, optional name, track count, and total
  duration. A header may later grow a short summary such as the primary
  orchestra, but that is not required for v1.
- A header is a queue command target, not a playable track. Loading or previewing
  from it is disabled; its child tracks retain all normal track actions.

### Classification and dissolution

- Making a tanda is offered only for a contiguous selection of ordinary track
  rows. It is disabled for an empty, non-contiguous, or partially overlapping
  selection.
- **Ungroup tanda** removes only the classification, never tracks.
- Any operation that changes a tanda's membership dissolves it immediately:
  removing a member, adding a track inside it, replacing a member, or moving an
  individual member into or out of it.
- Adding tracks later never recreates or extends a tanda. The DJ classifies the
  resulting consecutive block again.
- Moving the complete, unchanged tanda block preserves its classification and
  collapsed/expanded state.

### Queue safety

- With Auto DJ stopped, a whole tanda may move anywhere in the queue.
- While Auto DJ is running, a move may affect only the unplayed, un-loaded
  region. A move that contains a deck-loaded/current row, or inserts before the
  current cursor, is refused with a concise explanation. This keeps the current
  Tango cursor and deck handoff contract intact.
- Existing individual-track moves remain available for ungrouped rows. An
  individual move involving a tanda first dissolves that tanda.

## Data design (no schema migration)

Persist group definitions in TangoMode configuration, not in the Mixxx
database schema. The Auto DJ queue itself remains the existing hidden
`PlaylistTracks` playlist and continues to persist normally.

Introduce a small `TandaQueueState` service, owned by `AutoDJFeature`:

```cpp
enum class TandaType { Tango, Vals, Milonga, NuevoAlternative };

struct TandaSpan {
    QUuid id;
    TandaType type;
    QString name; // empty in v1 unless naming is exposed
    QVector<TrackId> members; // exact ordered occurrence sequence
    int anchorPosition;       // one-based queue position at last save
    bool collapsed;
};
```

The serialized state is a versioned compact JSON value under a TangoMode-owned
configuration key, for example `[TangoMode] AutoDjTandasV1`. This is metadata
for a local queue view; it does not alter the library database or Mixxx schema.

On startup, restore only spans whose complete ordered member sequence still
matches a consecutive queue range near its anchor. Discard an ambiguous or
invalid span rather than guessing. This is especially important because a
playlist can contain the same track more than once.

The state service maintains a before/after ordered queue snapshot. An ordinary
append leaves prior spans intact. Any unrecognised queue mutation invalidates
the affected span(s); deliberate whole-tanda moves are performed through the
service and preserve the moved span explicitly.

## UI and model design

The current queue uses `PlaylistTableModel` in `WTrackTableView`, which is a
flat table. Do not change that model into a tree and do not put tanda data into
the playback model.

Add a Tango-only adapter/view pair:

- `TandaQueueModel` exposes visible rows: either a virtual tanda-header row or
  a mapped Auto DJ playlist track row.
- For a track row it forwards columns, display data, delegates, and track
  operations to `PlaylistTableModel`.
- For a header row it supplies header-specific display/roles and no `TrackPointer`.
- `WTandaQueueView` builds on the existing track-table behaviour but handles
  header selection, expand/collapse, tanda context actions, and block drag/drop
  before any action reaches `WTrackTableView`'s leaf-row logic.

Use this same view/model instance pattern for both the main Auto DJ page and
the dockable Auto DJ Queue panel. They must share `TandaQueueState` and the
same underlying `PlaylistTableModel`, so a collapse, classification, or move in
one view is immediately reflected in the other.

The old flat view remains the fallback whenever Tango mode is off. This keeps
stock Mixxx behaviour untouched.

## Implementation stages

### 1. State service and pure tests

- Add `TandaQueueState` with span validation, classification, dissolution,
  expansion state, persistence, and queue-snapshot reconciliation.
- Add pure tests for contiguous selection validation, overlap rejection,
  exact-member restore, duplicate-track ambiguity, and the dissolution rules.
- Do not change the visible queue yet.

### 2. Explicit queue operations

- Add `AutoDJFeature` commands for make/ungroup/change type and move whole
  tanda up/down.
- Add a DAO-level atomic range-move operation for `PlaylistTracks`, or an
  equivalent transaction that emits one `tracksMoved` notification after the
  completed block move. Do not use the existing one-row move loop: each row
  currently refreshes the Auto DJ model independently.
- Route individual removal/reorder commands through the state service so an
  affected tanda is dissolved before the existing DAO operation.
- Preserve Tango's cursor/loaded-track protection when Auto DJ is running.
- Test group movement above/below tracks, at queue bounds, and around duplicate
  track IDs.

### 3. Collapsible outline view

- Implement `TandaQueueModel` and `WTandaQueueView`.
- Add clickable disclosure affordances and accessible action text for expand /
  collapse.
- Add the tanda context menu actions. The menu must only offer classification
  for valid consecutive leaf-row selections.
- Wire both Tango Auto DJ views to the adapter only while Tango mode is enabled.
- Verify selection, loading a child track, context menus, row colours, start
  markers, cortina marks, pause-after marks, scrolling, and queue refreshes.

### 4. Persistence and recovery

- Load state after the Auto DJ playlist model is populated.
- Save immediately after a classifying, dissolving, collapse, or block-move
  operation; do not defer it until shutdown.
- On restore, log and discard only malformed/unmatched spans, never alter the
  underlying queue to make metadata fit.
- Test close/reopen with an intact queue, an appended queue, a changed member,
  and duplicate tracks.

### 5. Playback-regression review

- Run the focused Auto DJ processor tests and add tests for cursor behaviour
  while moving a future tanda during a running Tango set.
- Manually verify that a collapsed tanda plays every child in order and that
  neither its type nor its collapse state affects gaps, cortinas, or transitions.
- Verify Tango off still exposes the unchanged flat stock Auto DJ interface.

## Non-goals for v1

- Saving a tanda as a reusable library template.
- Automatic tanda detection or auto-classification.
- Tanda-specific transition rules.
- Nested tandas or overlapping labels.
- Changing the Mixxx database schema.

Reusable tanda templates and tango-aware set history can build on this state
model later, but neither is required for a useful first release.
