// TangoQ, a Mixxx fork purpose-built for Argentine Tango DJs.
// Copyright © 2026 Seemanta Dutta (TangoQ).
//
// This file is part of TangoQ and is licensed under the GNU General Public
// License, version 2 or later. TangoQ is based on Mixxx (Copyright © 2001-2026
// the Mixxx Development Team); see the LICENSE file for the full text.

#include "library/autodj/autodjqueuereset.h"

#include <QMessageBox>
#include <QObject>

#include "control/controlobject.h"
#include "mixer/playerinfo.h"
#include "mixer/playermanager.h"
#include "preferences/configobject.h"

namespace mixxx {

bool canResetAutoDJQueueState() {
    const bool tangoMode =
            ControlObject::get(ConfigKey("[AutoDJ]", "keep_queue")) > 0.0;
    const bool autoDJRunning =
            ControlObject::get(ConfigKey("[AutoDJ]", "enabled")) > 0.0;
    const bool liveMode =
            ControlObject::get(ConfigKey("[AutoDJ]", "live_mode")) > 0.0;
    if (!tangoMode || autoDJRunning || liveMode) {
        return false;
    }
    // Auto DJ being off does not by itself mean the decks are idle: it stops as
    // the last track starts, and a deck can always be started by hand. Require
    // silence too rather than risk cutting a track still playing out.
    const int numDecks = PlayerInfo::instance().numDecks();
    for (int deck = 0; deck < numDecks; ++deck) {
        // PlayerManager::groupForDeck is 0-indexed.
        if (ControlObject::get(ConfigKey(PlayerManager::groupForDeck(deck),
                    QStringLiteral("play"))) > 0.0) {
            return false;
        }
    }
    return true;
}

void resetAutoDJQueueState(QWidget* pParent) {
    // Deliberate, confirmed action: restart the Tango set from the top. Marks
    // every queued track unplayed (reverting the grey "played" colour), resets
    // the play cursor to the first track and clears the decks, so a fully-played
    // set can be replayed from a genuinely clean slate.
    const auto answer = QMessageBox::question(pParent,
            QObject::tr("Eject decks and reset TangoQ state"),
            QObject::tr("Eject the tracks loaded on the decks, mark all tracks in "
                        "the Auto DJ queue as unplayed and restart the set from "
                        "the top?\n\nThis does not change your play counts."),
            QMessageBox::Yes | QMessageBox::Cancel,
            QMessageBox::Cancel);
    if (answer != QMessageBox::Yes) {
        return;
    }
    // Clear the decks first so nothing is left cued from the previous run. This
    // action is only offered while Auto DJ is stopped and outside LIVE mode, so
    // no deck can be ejected out from under a running set. Stop before eject to
    // match how the library handles ejecting a loaded deck.
    const int numDecks = PlayerInfo::instance().numDecks();
    for (int deck = 0; deck < numDecks; ++deck) {
        // PlayerManager::groupForDeck is 0-indexed.
        const QString group = PlayerManager::groupForDeck(deck);
        // Only decks that actually hold a track: the eject control doubles as
        // un-eject, so pressing it on an empty deck reloads the track that was
        // ejected last, which would put tracks back on the decks we are clearing.
        if (!PlayerInfo::instance().getTrackInfo(group)) {
            continue;
        }
        ControlObject::set(ConfigKey(group, QStringLiteral("stop")), 1.0);
        ControlObject::set(ConfigKey(group, QStringLiteral("eject")), 1.0);
        ControlObject::set(ConfigKey(group, QStringLiteral("eject")), 0.0);
    }
    // The AutoDJProcessor owns the play cursor and the queue model, so route the
    // reset through its control (it clears the played flags and the cursor).
    ControlObject::set(ConfigKey("[AutoDJ]", "reset_queue_state"), 1.0);
}

} // namespace mixxx
