// TangoQ, a Mixxx fork purpose-built for Argentine Tango DJs.
// Copyright © 2026 Seemanta Dutta (TangoQ).
//
// This file is part of TangoQ and is licensed under the GNU General Public
// License, version 2 or later. TangoQ is based on Mixxx (Copyright © 2001-2026
// the Mixxx Development Team); see the LICENSE file for the full text.

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
