// TangoQ, a Mixxx fork purpose-built for Argentine Tango DJs.
// Copyright © 2026 Seemanta Dutta (TangoQ).
//
// This file is part of TangoQ and is licensed under the GNU General Public
// License, version 2 or later. TangoQ is based on Mixxx (Copyright © 2001-2026
// the Mixxx Development Team); see the LICENSE file for the full text.

#pragma once

class QWidget;

namespace mixxx {

/// macOS only: the height, in points, of the strip at the top of the screen
/// where the hidden menu bar appears while pWidget's window is in full screen,
/// or 0 when it is not in full screen.
///
/// In full screen macOS hides the menu bar and slides it in when the pointer
/// reaches the top of the screen, and a click in that strip goes to the menu
/// bar even over one of our popup menus. Qt counts the strip as free space, so
/// a tall menu can be placed with its first item in it, where it cannot be
/// clicked. The strip is reported even while the bar is hidden.
int fullScreenMenuBarHeight(const QWidget* pWidget);

} // namespace mixxx
