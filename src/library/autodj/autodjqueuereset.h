#pragma once

class QWidget;

namespace mixxx {

// True when "Eject decks and reset AutoDJ queue state" may be offered: Auto DJ
// is in Tango mode (keep_queue), stopped (not enabled), not in LIVE mode, and no
// deck is currently playing - so it can never wipe a running set or eject a deck
// out from under one.
bool canResetAutoDJQueueState();

// Confirms with the user, then stops and ejects every loaded deck and resets the
// Tango play cursor and played flags (restarting the set from the top). A no-op
// if the user cancels. `pParent` owns the confirmation dialog.
void resetAutoDJQueueState(QWidget* pParent);

} // namespace mixxx
