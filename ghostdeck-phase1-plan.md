# Phase 1 Plan — GhostDeck C++ Listener

> Status: **DRAFT — awaiting sign-off.** No code yet. Open design questions at the bottom.
> HTTP/JSON stack decision (locked): **QTcpServer + QJsonDocument** (zero new deps; matches Mixxx conventions).
> Based on a reconnaissance survey of the Mixxx fork checkout (branch `docs/install-guide`). Verify `file:line` refs before coding.

## Scope
An embedded, self-threaded HTTP listener in the Mixxx fork that receives Auto DJ queue state on
`127.0.0.1:9876` and applies it to the standby machine's Auto DJ queue via existing library APIs —
never crashing Mixxx. `QTcpServer` + `QJsonDocument`, zero new dependencies.

## Design decision baked in: full-snapshot sync
Rather than per-track deltas, each message carries the **entire ordered queue** and the listener applies
it with `AutoDJSendLoc::REPLACE`. This is idempotent — a dropped or duplicated message self-heals on the
next one — which is the "boring and bulletproof" choice over delta reconciliation. It also matches the
Component 1 description ("reads current queue state… serializes"). The controller script sends a snapshot
on every track change.

## Wire format (refinement to the original handoff prompt)
The original success-criteria `curl` sends `{"track","artist","position"}`. Title/artist is unreliable for
resolution. Since library parity is assumed (same files, same paths), the payload carries the **file
location** — that is what `TrackDAO::resolveTrackIds` matches on. Proposed body:

```json
{
  "seq": 42,
  "queue": [
    {"location": "/Music/DArienzo/LaCumparsita.flac", "title": "La Cumparsita", "artist": "D'Arienzo"}
  ]
}
```

`title`/`artist` stay in the payload for logging and future parity-checker use; `location` is the source of
truth. `seq` lets the listener ignore stale/out-of-order messages.

## File structure — `src/ghostdeck/`
- `GhostDeckListener.{h,cpp}` — **main-thread** manager. Owns the `QThread`, owns the queue-apply logic
  (holds `TrackCollectionManager*`). Lifecycle: `start()` / `stop()`.
- `GhostDeckServer.{h,cpp}` — **worker**, lives on the `QThread`. Owns `QTcpServer`, accepts connections,
  parses the HTTP request + JSON, validates, then marshals to the manager. Never touches the library.
- `GhostDeckProtocol.{h,cpp}` — pure parse/serialize + validation of the JSON body (no Qt threading,
  unit-testable in isolation).

## Thread model & data flow (critical)
```
socket thread (QThread)                    main/GUI thread
─────────────────────                      ───────────────
QTcpServer.newConnection
  → read bytes, parse HTTP
  → QJsonDocument parse + validate
  → 400 on malformed (respond here)
  → QMetaObject::invokeMethod(  ─────────► GhostDeckListener::applyQueue(snapshot)
       listener, Qt::QueuedConnection)        (runs on main thread, DAO-safe)
  → respond 200 {"status":"ok"}            resolveTrackIds → addTracksToAutoDJQueue(REPLACE)
```
- The server object is **created inside the worker thread** (via a `requestInitialize` slot after
  `moveToThread`) so `QTcpServer` and its child sockets have correct thread affinity.
- **Ack model: fire-and-ack.** The worker validates the JSON synchronously (returns `400` on malformed),
  queues the apply, and returns `200 {"status":"ok"}`. It does *not* block on the main thread for the apply
  result — no `BlockingQueuedConnection`, so a busy GUI can never stall the socket thread. Queued
  connections preserve order, so snapshots apply in sequence.
- Lifecycle mirrors **`ControllerManager`** (`src/controllers/controllermanager.cpp:111-138`): `new QThread`
  → `moveToThread` → `emit requestInitialize`; on shutdown `emit requestShutdown(); m_pThread->wait()`.

## Queue-apply path (main thread, in `applyQueue`)
1. `m_pTCM->internalCollection()` → `getTrackDAO()` / `getPlaylistDAO()`.
2. `resolveTrackIds(fileInfos, ResolveTrackIdFlag::ResolveOnly)` (`src/library/dao/trackdao.h:53`) —
   unresolved entries logged, **never fatal** (library-parity constraint).
3. `playlistDao.addTracksToAutoDJQueue(resolvedIds, AutoDJSendLoc::REPLACE)`
   (`src/library/dao/playlistdao.h:101`, enum at `:26`).
4. Model refresh happens automatically via `PlaylistDAO`'s `playlistContentChanged` signals.
   *(Deliberately NOT calling `AutoDJProcessor` directly — no public accessor exists (`src/library/library.h:181`)
   and no add method; `PlaylistDAO` is the sanctioned path.)*

## Lifecycle wiring — `CoreServices`
- **Construct** in `CoreServices::initialize()` right after `Library` is bound (~`src/coreservices.cpp:585`),
  as a `std::unique_ptr` member alongside the other managers (`src/coreservices.h:122-147`), passing
  `m_pTrackCollectionManager.get()`.
- **Destroy** at the **top** of `finalize()`, next to `stopLibraryScan()` (`src/coreservices.cpp:789`) —
  before Library/TrackCollectionManager teardown — so the socket thread is provably dead before the DAOs
  disappear.

## CMake wiring
- Add the three `src/ghostdeck/*.cpp` to the engine sources list in `CMakeLists.txt`.
- No new `find_package` — `Qt Network` is already a component (`CMakeLists.txt:2802`). Link is inherited.

## Never-crash measures
- Every socket handler wrapped so no exception crosses into the GUI thread; all errors caught + logged via
  `qWarning`, connection closed cleanly.
- Localhost bind only: `server.listen(QHostAddress::LocalHost, 9876)`.
- Malformed/oversized bodies rejected with `400` + a `Content-Length` cap.
- Relay disconnect / no traffic = no-op; Mixxx plays on. Reconnect is just the next accepted connection.

## Success criteria — two verifiable milestones
**1a — Plumbing (exact curl from the handoff prompt):**
`curl -X POST http://127.0.0.1:9876/track-change -d '{...}'` → listener logs the payload on its own thread →
returns `{"status":"ok"}` → Mixxx keeps running. *(Proves the thread + socket + lifecycle without touching
the queue.)*

**1b — Queue apply:** POST a snapshot with real `location`s present in the library → Auto DJ queue visibly
updates in the GUI to match the snapshot → unknown locations logged, not crashed → Mixxx keeps running.

## Open questions for sign-off
1. **Full-snapshot + `REPLACE`** model — agree, or per-track deltas?
2. **`location` as the resolution key** (payload change from title/artist) — OK?
3. **Fire-and-ack** (don't block the socket thread on the apply result) — OK?
4. Endpoint shape `POST /track-change` — keep the name, or `POST /queue`?

## Reference — key survey findings (Mixxx fork, branch `docs/install-guide`)
- `AutoDJProcessor` ctor: `src/library/autodj/autodjprocessor.h:176-180`; owned privately by `Library`
  (`src/library/library.h:181`) — no public accessor to reach it.
- Auto DJ queue = hidden playlist type `PLHT_AUTO_DJ` named `"Auto DJ"` (`src/library/dao/playlistdao.h:19-24`,
  `src/library/dao/trackschema.h:11`).
- **Thread affinity is hard-asserted**: `TrackCollection`/DAO access pinned to the main thread
  (`src/library/trackcollection.h:60` + ~30 sites), backed by thread-local `QSqlDatabase`. Marshaling
  precedents: `src/library/trackloader.cpp:44`, `src/library/export/engineprimeexportjob.cpp:496`.
- Cross-thread-safe channel to Auto DJ: `[AutoDJ]` ControlObjects (`skip_next`, `add_random_track`,
  `enabled`, plus fork's `keep_queue`/`live_mode`).
- Deps: **cpp-httplib and nlohmann/json are NOT in the tree.** Qt Network IS linked
  (`CMakeLists.txt:2802`); Mixxx uses `QJsonDocument` (`src/network/jsonwebtask.cpp`).
- No existing in-process network *listener* in Mixxx (all network code is outbound). Closest architectural
  template: `ControllerManager` (external input → Mixxx via thread-safe ControlObjects).
