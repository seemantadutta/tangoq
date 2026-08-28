# TangoQ Semantic State Protocol (Experimental)

TangoQ publishes this read-only diagnostic protocol to describe observable
application state. It is not a remote-control API, a serialization of TangoQ
C++ objects, or a file/library transfer protocol.

The implementation and bundled monitor are GPL-licensed TangoQ code. The
protocol uses ordinary JSON over HTTP and WebSocket, so an independently
implemented client can consume it across a normal process/network boundary.
That engineering boundary is not a legal conclusion about any particular
client.

## Activation, transport, and security

The server is disabled by default. Start TangoQ with:

    --semantic-monitor-port <1-65535>

When the option is absent, TangoQ creates no semantic-monitor listening socket.
When enabled, it listens on all IPv4 interfaces so a device on the LAN can
connect. TangoQ logs the chosen port and available LAN addresses at startup.

The only endpoints are:

- GET /
- GET /api/state
- WebSocket GET /api/events

Only GET is accepted. There are no mutation endpoints, WebSocket commands,
static-file paths other than '/', filesystem access, or configuration changes.
Unsupported methods return 405; unknown paths return 404; a plain request to
/api/events returns 426 because it requires a WebSocket upgrade.

Version 2 has no authentication or transport encryption. Enabling it exposes
the documented session and track metadata to devices that can reach the chosen
port. Use it only on a trusted local network. This diagnostic feature is
auxiliary: clients must not be relied on for playback operation.

## Versioning, snapshots, and revisions

Every snapshot and WebSocket message includes schemaVersion. This document
defines version 2. A version may add optional members without changing existing
meanings. Clients must ignore members they do not understand. A change that
alters an existing field's meaning requires a new schema version.

GET /api/state is authoritative. A client may discard all local state and
replace it with that response at any time. WebSocket delivery is not an event
history and is not replayed after disconnect. Reconnecting clients receive a
fresh current snapshot immediately after the WebSocket handshake.

revision is a process-local, monotonically increasing integer. It begins at
zero when the publisher starts and increments exactly once only when the
serialized semantic snapshot changes. Connecting, disconnecting, or adding
other clients does not change it. A playback-position publication is attempted
at most four times per second; it advances the revision only if its serialized
value changed. Clients seeing a gap or a new session.id should fetch a fresh
snapshot rather than infer missing intermediate changes.

## Session identity

session.id is generated when TangoQ's semantic-state publisher starts, and
session.startedAt is that publisher's UTC start time in ISO 8601 format with
milliseconds. The identity remains stable while TangoQ is running, including
when Auto DJ is stopped or started, the queue is cleared, or tandas are edited.
Restarting TangoQ creates a new identity. It is a publisher/process identity,
not a milonga, event, library, or persisted user session identifier.

## Snapshot schema

The snapshot has required schemaVersion, revision, session, playback, queue,
and extensions.tangoq fields. All strings are JSON strings and therefore UTF-8
when encoded on the wire. Metadata may be empty. Optional/unknown scalar and
object fields are emitted as JSON null, never omitted.

### Generic fields

queue is the complete ordered Auto DJ queue. Each item contains a required
one-based position and a required track object. A track object always contains:

- id: opaque TangoQ library identifier
- artist: metadata string, possibly empty
- title: metadata string, possibly empty
- durationMs: non-negative duration in milliseconds, or null when unknown

Current TangoQ identifiers use the tangoq: prefix and distinguish library
records within a TangoQ library. They are not file hashes and do not guarantee
that different machines, databases, or library imports identify the same audio
content. Filesystem paths and audio data are never exposed.

playback always contains:

- state: stopped, paused, or playing
- track: track object or null
- queuePosition: one-based Auto DJ position or null
- positionMs: non-negative position in milliseconds or null

If exactly one loaded deck is playing, playback.track and positionMs describe
that deck. queuePosition is non-null only when that track is also the active
Auto DJ cursor item. If no deck is playing but a deck matching the active
cursor is loaded, playback is paused and describes that cursor item. Otherwise
playback is stopped and its optional fields are null.

If more than one deck is playing, state is playing but track, positionMs, and
queuePosition are null. TangoQ does not claim to identify a single
audible/program track in a manual multi-deck mix. TangoQ's Auto DJ cursor and
tanda/cortina extension state remain available separately in that case.

### TangoQ extension

Tango-specific information is under extensions.tangoq; generic clients can
ignore this object. It always contains tandas, currentTanda, upcomingTanda,
and cortina.

A tanda object has required id, type, name, startPosition, and trackCount.
startPosition and currentTanda.trackIndex are one-based. trackCount describes
the span TangoQ currently knows, including partial/incomplete tandas. type uses
TangoQ's current semantic names, including tango, vals, milonga, and
nuevo-alternative.

currentTanda is non-null only when the active Auto DJ cursor falls within a
known tanda span. Its trackIndex is the cursor's one-based offset in that span.
upcomingTanda is the first known tanda span beginning after that cursor. If the
cursor is absent, exhausted, manually bypassed, or outside known spans, both
values are null; the protocol deliberately does not guess from retained queue
entries.

cortina always contains state, queuePosition, and track. state is current when
the active cursor item is tagged by TangoQ as a cortina, upcoming for the first
tagged item strictly after the active cursor, and none otherwise. For current
and upcoming, queuePosition and track are non-null. With no valid active
cursor, state is none and both fields are null. Cortina classification reflects
TangoQ's current queue/tag state, so queue edits can change it immediately.

## WebSocket messages

Immediately after a successful handshake, TangoQ sends a message with
schemaVersion 2, type snapshot, its revision, and a complete snapshot.
Subsequent messages use type state.changed with the same fields. The embedded
snapshot is complete and authoritative. state.changed says only that observable
state changed; it does not classify why. Clients must not derive state from a
typed-event history.

Multiple HTTP and WebSocket clients are independent and receive the same
current snapshots. Slow or malformed clients are bounded and may be
disconnected. Browser/network work is performed off the real-time audio
thread; TangoQ coalesces position sampling rather than sending engine timing
updates directly to clients.

## Compatibility and exclusions

The protocol is experimental. Independent clients should tolerate reconnects,
missing/unknown optional values, and additional fields. TangoQ remains fully
useful with the publisher disabled and requires no client installation.

The protocol contains no C++ type names, pointers, widgets, database schema,
file paths, RPC methods, audio, caches, synchronization roles, recovery
policy, licensing, telemetry, or client-specific behavior.

## Manual semantic validation

Use the monitor while performing realistic work: build a tanda, start
playback, alter the next tanda mid-track, insert after the cursor, remove an
upcoming item, rapidly reorder items, pause/resume, skip, manually load
another deck, insert/remove a cortina, and allow natural tanda/cortina
transitions. Disconnect the phone, make several changes, reconnect, and
compare the returned snapshot directly with TangoQ. In each case, validate the
published facts rather than relying on event history.
