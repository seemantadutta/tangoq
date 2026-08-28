# TangoQ Semantic State Protocol (Experimental)

This document defines the version 1 diagnostic protocol published by TangoQ.
It describes observable DJ-session semantics, not TangoQ C++ objects or a
remote-control API.

The implementation and monitor page in the TangoQ repository are GPL-licensed
TangoQ code. The protocol uses ordinary JSON over HTTP and WebSocket so an
independently implemented process can consume it without linking to TangoQ.
This boundary is an intentionally conservative engineering design and is not a
legal conclusion about any particular client.

## Activation and transport

The server is disabled by default. Start TangoQ with
`--semantic-monitor-port <1-65535>` to listen on all IPv4 interfaces. When the
option is absent, TangoQ creates no semantic-monitor listening socket.

The read-only endpoints are:

- `GET /` - the bundled diagnostic monitor
- `GET /api/state` - the current authoritative snapshot
- WebSocket `/api/events` - a fresh snapshot followed by live state changes

There are no mutation endpoints or WebSocket commands. Clients reconnect by
obtaining a new snapshot; event history and replay are not provided.

Version 1 has no authentication or transport encryption. Enabling it exposes
the documented track/session metadata to devices that can reach the selected
port, so it is intended only for a trusted local network.

## Versioning and ordering

Every message includes `schemaVersion`. Version 1 additions may introduce new
optional object members. Consumers should ignore members they do not know.

Every snapshot includes a monotonically increasing `revision` scoped to its
`session.id`. A state change increments the revision exactly once. Publishing
an identical state does not increment it. Clients that observe a revision gap
should fetch `GET /api/state` rather than infer missing state.

## Snapshot

The top-level generic fields are:

- `session`: process-session identifier and UTC start time
- `playback`: `stopped`, `paused`, or `playing`, current semantic track,
  queue position, position in milliseconds, and duration in milliseconds
- `queue`: the complete ordered Auto DJ queue
- `extensions.tangoq`: TangoQ-specific tanda and cortina semantics

Track identifiers are opaque, source-qualified strings. Version 1 TangoQ IDs
use the `tangoq:` prefix. A track contains only identifier, artist, title, and
duration. File locations and audio data are never published.

Queue positions and tanda `startPosition` values are one-based. A current
tanda's `trackIndex` is also one-based. Tango types use TangoQ's semantic names:
`tango`, `vals`, `milonga`, and `nuevo-alternative`.

`extensions.tangoq.tandas` describes grouping spans without changing the
generic queue shape. `currentTanda` and `upcomingTanda` are nullable. Cortina
state is `none`, `upcoming`, or `current` and includes its queue position and
track when available.

## WebSocket messages

Immediately after the WebSocket handshake the server sends:

```json
{
  "schemaVersion": 1,
  "type": "snapshot",
  "revision": 12,
  "snapshot": { "schemaVersion": 1, "revision": 12 }
}
```

Subsequent changes use:

```json
{
  "schemaVersion": 1,
  "type": "state.changed",
  "change": "queue.tracksAdded",
  "revision": 13,
  "snapshot": { "schemaVersion": 1, "revision": 13 }
}
```

The complete resulting snapshot is included. An independent consumer may
replace its local view atomically or derive its own state without replaying
TangoQ-specific operations. Change names are informative and include
`queue.tracksAdded`, `queue.tracksRemoved`,
`queue.tracksReordered`, `playback.trackLoaded`, `playback.trackUnloaded`,
`playback.stateChanged`, `playback.positionChanged`,
`extensions.tangoq.tandasChanged`, and
`extensions.tangoq.cortinasChanged`.

Playback position changes are coalesced to at most four publication attempts
per second. Identical states are suppressed by the semantic store.

## Boundary and exclusions

The protocol contains no C++ type names, pointers, widget state, database
schema, file paths, RPC methods, audio, caches, synchronization, failover,
licensing, telemetry, or product-specific client behavior. TangoQ does not
require any client to be installed and remains fully functional with the
publisher disabled.
