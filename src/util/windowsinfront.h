// TangoQ, a Mixxx fork purpose-built for Argentine Tango DJs.
// Copyright © 2026 Seemanta Dutta (TangoQ).
//
// This file is part of TangoQ and is licensed under the GNU General Public
// License, version 2 or later. TangoQ is based on Mixxx (Copyright © 2001-2026
// the Mixxx Development Team); see the LICENSE file for the full text.

#pragma once

#include <QStringList>

class QWidget;

namespace mixxx {

/// macOS only: describes every on-screen window, of any app, that is in front
/// of pWindow and overlaps it: its owner, layer, opacity and bounds. Such a
/// window takes the clicks meant for pWindow even when it draws nothing, which
/// may be why clicks on the track menu's first item have twice not reached it.
/// Empty when nothing is in the way, or when the window is not shown.
QStringList windowsInFrontOf(const QWidget* pWindow);

} // namespace mixxx
