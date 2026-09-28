// TangoQ, a Mixxx fork purpose-built for Argentine Tango DJs.
// Copyright © 2026 Seemanta Dutta (TangoQ).
//
// This file is part of TangoQ and is licensed under the GNU General Public
// License, version 2 or later. TangoQ is based on Mixxx (Copyright © 2001-2026
// the Mixxx Development Team); see the LICENSE file for the full text.

#include "util/windowsinfront.h"

#import <AppKit/AppKit.h>
#import <CoreGraphics/CoreGraphics.h>

#include <QRect>
#include <QWidget>
#include <QWindow>

namespace mixxx {

QStringList windowsInFrontOf(const QWidget* pWindow) {
    QStringList result;
    if (!pWindow || !pWindow->isVisible() || !pWindow->windowHandle()) {
        return result;
    }
    NSView* pView = (__bridge NSView*)reinterpret_cast<void*>(
            pWindow->windowHandle()->winId());
    NSWindow* pNSWindow = pView.window;
    if (!pNSWindow || pNSWindow.windowNumber <= 0) {
        return result;
    }
    // Front to back, only the windows above ours. Owner names and bounds do
    // not need the Screen Recording permission (window titles would).
    NSArray* windows = CFBridgingRelease(
            CGWindowListCopyWindowInfo(kCGWindowListOptionOnScreenAboveWindow |
                            kCGWindowListExcludeDesktopElements,
                    static_cast<CGWindowID>(pNSWindow.windowNumber)));
    // Global coordinates in points, top-left origin: the same as Qt's.
    const QRect ours = pWindow->frameGeometry();
    for (NSDictionary* info in windows) {
        CGRect bounds;
        if (!CGRectMakeWithDictionaryRepresentation(
                    (__bridge CFDictionaryRef)
                            info[(__bridge NSString*)kCGWindowBounds],
                    &bounds)) {
            continue;
        }
        const QRect theirs(static_cast<int>(bounds.origin.x),
                static_cast<int>(bounds.origin.y),
                static_cast<int>(bounds.size.width),
                static_cast<int>(bounds.size.height));
        if (!theirs.intersects(ours)) {
            continue;
        }
        NSString* owner = info[(__bridge NSString*)kCGWindowOwnerName];
        NSNumber* pid = info[(__bridge NSString*)kCGWindowOwnerPID];
        NSNumber* layer = info[(__bridge NSString*)kCGWindowLayer];
        NSNumber* alpha = info[(__bridge NSString*)kCGWindowAlpha];
        result << QStringLiteral(
                "%1 (pid %2), layer %3, alpha %4, at %5,%6 %7x%8")
                          .arg(owner ? QString::fromNSString(owner)
                                     : QStringLiteral("?"))
                          .arg(pid.intValue)
                          .arg(layer.intValue)
                          .arg(alpha.doubleValue)
                          .arg(theirs.x())
                          .arg(theirs.y())
                          .arg(theirs.width())
                          .arg(theirs.height());
    }
    return result;
}

} // namespace mixxx
