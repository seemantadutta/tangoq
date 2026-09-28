// TangoQ, a Mixxx fork purpose-built for Argentine Tango DJs.
// Copyright © 2026 Seemanta Dutta (TangoQ).
//
// This file is part of TangoQ and is licensed under the GNU General Public
// License, version 2 or later. TangoQ is based on Mixxx (Copyright © 2001-2026
// the Mixxx Development Team); see the LICENSE file for the full text.

#include "util/macosfullscreen.h"

#import <AppKit/AppKit.h>

#include <QWidget>
#include <QWindow>
#include <cmath>

namespace mixxx {

int fullScreenMenuBarHeight(const QWidget* pWidget) {
    const QWidget* pWindow = pWidget ? pWidget->window() : nullptr;
    if (!pWindow || !pWindow->isFullScreen()) {
        return 0;
    }
    // The main menu reports its bar's height even while full screen hides it
    // (30 points on a MacBook without a notch, where NSStatusBar says 22).
    CGFloat height = NSApp.mainMenu.menuBarHeight;
    // On a display with a camera notch the bar is as tall as the notch area.
    if (@available(macOS 12.0, *)) {
        if (pWindow->windowHandle()) {
            NSView* pView = (__bridge NSView*)reinterpret_cast<void*>(
                    pWindow->windowHandle()->winId());
            NSScreen* pScreen = pView.window.screen;
            if (pScreen) {
                height = std::max(height, pScreen.safeAreaInsets.top);
            }
        }
    }
    return static_cast<int>(std::ceil(height));
}

} // namespace mixxx
