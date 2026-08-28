# Semantic State Monitor Spike Handoff

## Purpose

This spike adds a small, experimental, read-only semantic-state monitor to
TangoQ. TangoQ publishes observable DJ-session state over local HTTP and
WebSocket endpoints so an independent browser or future external application
can monitor the current session.

The deliverable is a durable semantic boundary, not a backup, playback, or
control system. TangoQ says what is happening; external software decides what,
if anything, to do with that information.

## Licensing and Product Boundary

TangoQ is GPL v2-or-later software derived from Mixxx. Everything in this
repository is intended to be suitable for the GPL-covered TangoQ codebase.

The implementation deliberately keeps a normal process and network boundary:

    TangoQ models/controllers
            |
    semantic-state adapter/store
            |
    HTTP and WebSocket messages
            |
    independent client process

The monitor contains no proprietary code, external product integration,
external SDK, shared-memory exchange, C++ object serialization, or plugin
mechanism. It neither requires nor loads another application. No definitive
legal conclusion about derivative works is implied; the design is
conservatively separated at a documented network protocol.

The API is monitoring-only. It exposes no endpoint for playback, queue,
library, configuration, file, or remote-execution control.

## Current Git State

- Original worktree: `/Users/seemanta/Projects/mixxx`
- Original branch: `main`
- Original HEAD and feature base:
  `49e484b5b3f271be9c2493898c280badb45180e9`
- Isolated feature worktree:
  `/Users/seemanta/Projects/tangoq-semantic-state`
- Isolated branch: `semantic-state-spike`
- Feature commit:
  `7499deb7a2a99e2386263db542e6385c35a5f56a`
  (`Add experimental semantic state monitor`)

The original worktree remains clean and unchanged. Do not merge this branch
into `main` without an explicit review and integration decision.

## Implemented Architecture

    Existing TangoQ models/controllers
                    |
                    v
       TangoQSemanticStateAdapter
                    |
                    v
          SemanticStateStore
              |          |
              v          v
         snapshot      revisions/events
                    |
                    v
        SemanticStateServer
              |          |
              v          v
            HTTP     WebSocket

The adapter consumes authoritative application and AutoDJ state rather than
scraping visible widgets. The state store owns a complete current snapshot and
a monotonically increasing revision. The network server consumes only the
semantic store, which keeps HTTP/WebSocket mechanics separate from TangoQ
models.

## Protocol and Behavior

- `GET /api/state` returns a complete authoritative JSON snapshot.
- `WebSocket /api/events` pushes live state updates to connected clients.
- A newly connected or reconnected client receives a fresh snapshot. There is
  no event persistence or replay requirement.
- Snapshots include generic session, playback, and queue concepts.
- TangoQ-specific tanda and cortina information lives under
  `extensions.tangoq`, keeping generic DJ concepts distinct from tango-specific
  semantics.
- Revisions advance only when semantic state changes, allowing clients to
  identify ordering and staleness.
- Playback position is coalesced to roughly four updates per second. Network
  serialization and delivery do not run in the real-time audio thread.

The protocol is documented in `docs/semantic-state-protocol.md`.

## User-Facing Monitor

The minimal static browser monitor is served from `/` and implemented in
`res/semanticmonitor/index.html`. It shows connection status, session ID,
revision, playback, current track, position, queue, tango state when present,
and recent semantic event information. It updates without a manual refresh.

The monitor is intentionally diagnostic rather than a future polished client.

## Enabling the Feature

The server is disabled by default and creates no listening socket unless the
command-line option is supplied:

    ./build/TangoQ.app/Contents/MacOS/TangoQ --semantic-monitor-port 39087

When enabled, TangoQ logs the selected port and usable LAN address(es). A phone
or browser on the same LAN can open:

    http://<TangoQ-LAN-IP>:39087/

## Relevant Source Locations

- `src/semanticstate/semanticstatemodel.*`: protocol-facing semantic model and
  JSON serialization.
- `src/semanticstate/semanticstatestore.*`: authoritative snapshot and revision
  store.
- `src/semanticstate/tangoqsemanticstateadapter.*`: translation from TangoQ
  model/controller state into semantic state.
- `src/semanticstate/network/semanticstateserver.*`: local HTTP/WebSocket
  transport and static monitor serving.
- `src/test/semanticstatestore_test.cpp`: serialization, namespace, revision,
  and snapshot consistency tests.
- `src/test/semanticstateserver_test.cpp`: opt-in/read-only HTTP, multiple
  client, event delivery, and reconnect-snapshot tests.

## Validation Completed

- Fresh build succeeded with `ninja -C build mixxx-test mixxx`.
- Focused test run succeeded:

      QT_QPA_PLATFORM=offscreen ./build/mixxx-test \
        '--gtest_filter=SemanticState*:AutoDJProcessorTest.*'

  Result: 49 tests passed: 5 semantic-state tests and 44 AutoDJ regression
  tests.
- Manual local HTTP smoke tests verified that the server responds when enabled
  and the port refuses connections when disabled.
- Manual phone-browser testing succeeded.

## Scope Explicitly Excluded

Do not add any of the following as part of this spike:

- audio or track-file transfer
- file, library, phone-cache, or laptop synchronization
- emergency playback, recovery, failover, or standby operation
- remote controls for play, pause, skip, load, queue, reorder, or delete
- configuration changes through the browser
- accounts, cloud services, telemetry, payments, or licensing
- proprietary functionality or product-specific integration

## Areas to Resolve Before Broadening the Feature

1. Session identity and lifecycle: define precisely when the semantic session
   ID changes, for example application launch, explicit new event, queue reset,
   or AutoDJ state change.
2. Stable v1 contract: identify which fields are contractual and which remain
   experimental. Favor simple optional fields over a large extensibility system.
3. Event vocabulary: decide whether clients need typed events such as
   `queueChanged` and `tandaChanged`, or whether revisioned state snapshots are
   enough for v1.
4. Tanda/cortina transition semantics: specify meanings of current, upcoming,
   and track index during pauses, manual deck loads, transitions, and partial
   queues.
5. Multiple-deck policy: establish which deck is represented as program output
   outside AutoDJ and how ambiguous manual mixing is exposed.
6. LAN security posture: decide whether LAN binding remains appropriate, whether
   loopback should be the default, and whether a lightweight read-only pairing
   token is required in a later version.
7. Configuration UX: decide whether the command-line option remains sufficient
   or whether an experimental preference should be added. Preserve
   disabled-by-default behavior.
8. Client-neutral protocol documentation: document schema stability, optional
   fields, reconnection rules, timing expectations, and the rule that snapshots
   are authoritative.

## Recommended Next Steps

First validate semantics in real TangoQ workflows: empty queue, queue edits,
track loads, play/pause/resume, track transitions, tanda/cortina transitions,
two concurrent LAN clients, and reconnection after state changes. Then settle
session lifecycle, transition meanings, and the v1 protocol contract before
adding fields or configuration UI.

Keep the semantic adapter generic enough to describe DJ state while retaining
TangoQ-specific concepts under `extensions.tangoq`. Any future client should
remain independently implemented and should rely only on the documented
network messages.

## Identifier Hygiene

The current source tree, documentation, branch name, worktree name, current
Git refs, and regenerated build artifacts were audited and do not contain the
future product identifier. Inherited Git history predating this branch still
contains legacy references. Do not rewrite shared `main` history or local
reflogs without explicit authorization.
