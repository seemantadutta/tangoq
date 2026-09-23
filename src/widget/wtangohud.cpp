// TangoQ, a Mixxx fork purpose-built for Argentine Tango DJs.
// Copyright © 2026 Seemanta Dutta (TangoQ).
//
// This file is part of TangoQ and is licensed under the GNU General Public
// License, version 2 or later. TangoQ is based on Mixxx (Copyright © 2001-2026
// the Mixxx Development Team); see the LICENSE file for the full text.

#include "widget/wtangohud.h"

#include <QDateTime>
#include <QFontMetrics>
#include <QLocale>
#include <QMainWindow>
#include <QPainter>
#include <QString>
#include <cmath>

#include "control/controlproxy.h"
#include "library/autodj/tandaprogresspip.h"
#include "preferences/configobject.h"

namespace {
// Brand palette, matching the TangoQ logo and the rest of the HUD.
const QColor kColorText(0xe8, 0xec, 0xf6);           // white: countdown
const QColor kColorAccent(0xe6, 0x31, 0x4e);         // red: pips
const QColor kColorAccentDim(0xe6, 0x31, 0x4e, 110); // dimmed red: previewed pips
// Timing columns: muted captions, and warm values that tie in with the orange
// TangoQ and Fade Now buttons while staying distinct from the white countdown.
const QColor kColorColumnLabel(0x9a, 0xa3, 0xb5);
const QColor kColorColumnValue(0xf2, 0xc3, 0x8b);
// Over/under the target end time: coral when running over, mint when on time
// or early. Brighter than plain red/green so they read on the dark toolbar.
const QColor kColorOver(0xff, 0x6b, 0x6b);
const QColor kColorUnder(0x6f, 0xd3, 0x9a);

// The label rides above the time; the time is large so the countdown is hard to
// miss across a dim room. The track pips sit centered below the time, so the
// whole HUD reads as one centered stack rather than an off-axis cluster.
constexpr int kLabelPixelSize = 13;
constexpr int kTimePixelSize = 24;
constexpr int kSidePadding = 20;
// Timing column values are smaller than the countdown so it stays dominant.
constexpr int kValuePixelSize = 18;
// Gap between the countdown stack and each timing column.
constexpr int kColumnGap = 32;
// Vertical gap between the time and the pip row.
constexpr int kPipRowGap = 3;
// Breathing room above and below the stacked lines.
constexpr int kVerticalPadding = 4;

// Final-30 s "breathe": a calm sinusoidal in-out of the time value between a
// faint and a full red, one cycle every kBreathPeriodMs.
constexpr double kTwoPi = 6.283185307179586;
constexpr double kBreathPeriodMs = 2400.0;
constexpr int kBreathMinAlpha = 90; // faintest point of the breath (still legible)

const QString kGroup = QStringLiteral("[AutoDJ]");

QString formatCountdown(double seconds) {
    if (seconds < 0.0) {
        return QStringLiteral("--:--");
    }
    const int total = static_cast<int>(seconds + 0.5);
    return QString::asprintf("%02d:%02d", total / 60, total % 60);
}

QString countdownLabel(int nextKind) {
    switch (nextKind) {
    case 1:
        return QStringLiteral("Cortina in");
    case 2:
        return QStringLiteral("Set ends in");
    case 3:
        return QStringLiteral("Paused");
    case 4:
        return QStringLiteral("Pause in");
    default:
        return QStringLiteral("Next track in");
    }
}

int countdownLabelWidth(const QFontMetrics& fm) {
    return qMax(fm.horizontalAdvance(QStringLiteral("Next track in")),
            qMax(fm.horizontalAdvance(QStringLiteral("Cortina in")),
                    qMax(fm.horizontalAdvance(QStringLiteral("Set ends in")),
                            qMax(fm.horizontalAdvance(QStringLiteral("Paused")),
                                    fm.horizontalAdvance(QStringLiteral("Pause in"))))));
}

int countdownTimeCellWidth(const QFontMetrics& fm) {
    int width = fm.horizontalAdvance(QChar(':'));
    for (ushort c = '0'; c <= '9'; ++c) {
        width = qMax(width, fm.horizontalAdvance(QChar(c)));
    }
    return qMax(width, fm.horizontalAdvance(QChar('-')));
}

int countdownTimeWidth(const QFontMetrics& fm) {
    return countdownTimeCellWidth(fm) * 5;
}

void drawFixedWidthTime(QPainter* p,
        const QRect& rect,
        const QFontMetrics& fm,
        const QString& time) {
    const int cellWidth = countdownTimeCellWidth(fm);
    for (int i = 0; i < time.size() && i < 5; ++i) {
        p->drawText(QRect(rect.left() + i * cellWidth, rect.top(), cellWidth, rect.height()),
                Qt::AlignCenter,
                QString(time.at(i)));
    }
}

QFont labelFont(const QFont& base) {
    QFont f = base;
    f.setPixelSize(kLabelPixelSize);
    f.setBold(true);
    return f;
}

QFont timeFont(const QFont& base) {
    QFont f = base;
    f.setPixelSize(kTimePixelSize);
    f.setBold(true);
    return f;
}

// Column captions are regular weight so they read as captions, not values.
QFont columnLabelFont(const QFont& base) {
    QFont f = base;
    f.setPixelSize(kLabelPixelSize);
    f.setBold(false);
    return f;
}

QFont valueFont(const QFont& base) {
    QFont f = base;
    f.setPixelSize(kValuePixelSize);
    f.setBold(true);
    return f;
}

// A duration as H:MM:SS, e.g. "3:42:10" or "0:04:12".
QString formatHms(qint64 totalSeconds) {
    return QString::asprintf("%lld:%02lld:%02lld",
            totalSeconds / 3600,
            (totalSeconds % 3600) / 60,
            totalSeconds % 60);
}

QString formatEndDelta(qint64 deltaSeconds) {
    if (deltaSeconds == 0) {
        return QStringLiteral("On time");
    }
    // Positive means the set ends after the target (running over).
    return deltaSeconds > 0
            ? QStringLiteral("%1 over").arg(formatHms(deltaSeconds))
            : QStringLiteral("%1 under").arg(formatHms(-deltaSeconds));
}

// Widest texts each column can show, for a stable reserved width.
const QString kSetLengthLabel = QStringLiteral("Set length");
const QString kEndsAtLabel = QStringLiteral("Ends at");
const QString kWidestValue = QStringLiteral("88:88:88");
const QString kWidestDelta = QStringLiteral("88:88:88 under");

// A clock time in the operating system's short format, the same format the
// toolbar clock (WTime) uses, e.g. "11:47 PM" or "23:47".
QString formatClockTime(const QTime& time) {
    return QLocale().toString(time, QLocale::ShortFormat);
}

// The widest clock text the locale can produce, for a stable column width.
QString widestClockTime() {
    QString text = formatClockTime(QTime(23, 58));
    for (QChar& c : text) {
        if (c.isDigit()) {
            c = QLatin1Char('8');
        }
    }
    return text;
}
} // namespace

WTangoHud::WTangoHud(QWidget* pParent)
        : WWidget(pParent) {
    const auto makeProxy = [this](const QString& key) {
        auto* pProxy = new ControlProxy(ConfigKey(kGroup, key), this);
        pProxy->connectValueChanged(this, &WTangoHud::slotControlChanged);
        return pProxy;
    };
    m_pCountdownSeconds = makeProxy(QStringLiteral("hud_countdown_seconds"));
    m_pNextKind = makeProxy(QStringLiteral("hud_next_kind"));
    m_pTandaTrackCount = makeProxy(QStringLiteral("hud_tanda_track_count"));
    m_pTandaPlayingIndex = makeProxy(QStringLiteral("hud_tanda_playing_index"));
    m_pAutoDJEnabled = makeProxy(QStringLiteral("enabled"));

    // The show/hide toggles live in the [TangoQ] group, driven from the Settings
    // panel. Repaint when either flips so the HUD updates live.
    const auto makeTangoQProxy = [this](const QString& key) {
        auto* pProxy = new ControlProxy(
                ConfigKey(QStringLiteral("[TangoQ]"), key), this);
        pProxy->connectValueChanged(this, &WTangoHud::slotControlChanged);
        return pProxy;
    };
    m_pShowCountdownTimer = makeTangoQProxy(QStringLiteral("show_countdown_timer"));
    m_pShowProgressPips = makeTangoQProxy(QStringLiteral("show_progress_pips"));
    m_pShowSetTime = makeTangoQProxy(QStringLiteral("show_adj_set_time"));
    m_pShowEndTime = makeTangoQProxy(QStringLiteral("show_adj_end_time"));
    m_pSetLengthSeconds = makeProxy(QStringLiteral("hud_set_length_seconds"));
    m_pSetEndEpochSeconds = makeProxy(QStringLiteral("hud_set_end_epoch_seconds"));
    m_pSetEndDeltaSeconds = makeProxy(QStringLiteral("hud_set_end_delta_seconds"));

    // Final-30 s breathe: advance the phase at ~25 fps and repaint. Runs only
    // while inside the window (started/stopped from slotControlChanged), so the
    // extra repaints cost nothing the rest of the time.
    m_flashTimer.setInterval(40);
    connect(&m_flashTimer, &QTimer::timeout, this, [this]() {
        m_breathPhase += m_flashTimer.interval() / kBreathPeriodMs;
        if (m_breathPhase >= 1.0) {
            m_breathPhase -= 1.0;
        }
        update();
    });
}

void WTangoHud::setup(const QDomNode& node, const SkinContext& context) {
    Q_UNUSED(node);
    Q_UNUSED(context);
    // No skin-configurable properties yet; object name and connections are
    // handled by commonWidgetSetup().
}

bool WTangoHud::inFlashWindow() const {
    const double seconds = m_pCountdownSeconds->get();
    return seconds >= 0.0 && seconds < 30.0;
}

void WTangoHud::slotControlChanged(double value) {
    Q_UNUSED(value);
    // The content width can change (label text, pip count), so re-query the
    // layout as well as repaint. This keeps the HUD from ever clipping.
    updateGeometry();
    // Run the breathe timer only during the final-30 s window.
    if (inFlashWindow()) {
        if (!m_flashTimer.isActive()) {
            m_breathPhase = 0.0; // start faint and breathe up
            m_flashTimer.start();
        }
    } else if (m_flashTimer.isActive()) {
        m_flashTimer.stop();
    }
    update();
}

int WTangoHud::contentWidth() const {
    const QFontMetrics lm(labelFont(font()));
    const QFontMetrics tm(timeFont(font()));

    const int trackCount = static_cast<int>(m_pTandaTrackCount->get());
    const int pipD = mixxx::kTandaProgressPipDiameter;
    const int pipGap = mixxx::kTandaProgressPipGap;
    const int pipsWidth =
            trackCount > 0 ? trackCount * pipD + (trackCount - 1) * pipGap : 0;

    // The stack is as wide as its widest row (label, time, or pips).
    const int contentW = qMax(qMax(countdownLabelWidth(lm), countdownTimeWidth(tm)),
            pipsWidth);
    return contentW + kSidePadding;
}

int WTangoHud::leftColumnWidth() const {
    const QFontMetrics lm(columnLabelFont(font()));
    const QFontMetrics vm(valueFont(font()));
    return qMax(lm.horizontalAdvance(kSetLengthLabel), vm.horizontalAdvance(kWidestValue));
}

int WTangoHud::rightColumnWidth() const {
    const QFontMetrics lm(columnLabelFont(font()));
    const QFontMetrics dm(labelFont(font()));
    const QFontMetrics vm(valueFont(font()));
    return qMax(qMax(lm.horizontalAdvance(kEndsAtLabel),
                        vm.horizontalAdvance(widestClockTime())),
            dm.horizontalAdvance(kWidestDelta));
}

void WTangoHud::paintTimingColumns(
        QPainter* p, int centerX, int stackWidth, int stackTop) {
    const bool showSetTime = m_pShowSetTime->get() > 0.0;
    const bool showEndTime = m_pShowEndTime->get() > 0.0;
    if (!showSetTime) {
        // "End time" alone has nothing to compare against without the
        // projected end, which rides with "Set time".
        return;
    }
    const QFont lf = columnLabelFont(font());
    const QFont df = labelFont(font());
    const QFont vf = valueFont(font());
    const QFontMetrics lm(lf);
    const QFontMetrics vm(vf);
    const int labelH = lm.height();
    const int valueH = vm.height();

    // A column that would not fit inside the HUD is skipped entirely. The
    // countdown is off-center, so one side can run short on a narrow window.
    const auto fits = [this](int x, int columnWidth) {
        return x >= 0 && x + columnWidth <= width();
    };

    // Left column: the whole set's length. Shown before the set starts too.
    const double lengthSeconds = m_pSetLengthSeconds->get();
    const int leftWidth = leftColumnWidth();
    const int leftX = centerX - stackWidth / 2 - kColumnGap - leftWidth;
    if (lengthSeconds >= 0.0 && fits(leftX, leftWidth)) {
        p->setFont(lf);
        p->setPen(kColorColumnLabel);
        p->drawText(QRect(leftX, stackTop, leftWidth, labelH),
                Qt::AlignHCenter | Qt::AlignVCenter,
                kSetLengthLabel);
        p->setFont(vf);
        p->setPen(kColorColumnValue);
        p->drawText(QRect(leftX, stackTop + labelH, leftWidth, valueH),
                Qt::AlignHCenter | Qt::AlignVCenter,
                formatHms(static_cast<qint64>(lengthSeconds)));
    }

    // Right column: the projected end, only while the set is running.
    const double endEpochSeconds = m_pSetEndEpochSeconds->get();
    const int rightWidth = rightColumnWidth();
    const int rightX = centerX + (stackWidth - stackWidth / 2) + kColumnGap;
    if (endEpochSeconds < 0.0 || !fits(rightX, rightWidth)) {
        return;
    }
    const QDateTime end = QDateTime::fromMSecsSinceEpoch(
            static_cast<qint64>(endEpochSeconds * 1000.0));
    p->setFont(lf);
    p->setPen(kColorColumnLabel);
    p->drawText(QRect(rightX, stackTop, rightWidth, labelH),
            Qt::AlignHCenter | Qt::AlignVCenter,
            kEndsAtLabel);
    p->setFont(vf);
    p->setPen(kColorColumnValue);
    p->drawText(QRect(rightX, stackTop + labelH, rightWidth, valueH),
            Qt::AlignHCenter | Qt::AlignVCenter,
            formatClockTime(end.time()));
    if (showEndTime) {
        const auto delta = static_cast<qint64>(m_pSetEndDeltaSeconds->get());
        // The over/under line keeps the bold caption font: it is a value.
        p->setFont(df);
        p->setPen(delta > 0 ? kColorOver : kColorUnder);
        p->drawText(QRect(rightX, stackTop + labelH + valueH, rightWidth, labelH),
                Qt::AlignHCenter | Qt::AlignVCenter,
                formatEndDelta(delta));
    }
}

int WTangoHud::stackCenterX(int stackWidth) const {
    // The deck waveforms span the skin, which is the main window's central
    // widget (a docked TangoQ panel sits beside it), and the play marker sits
    // in their middle. Center there so the countdown reads as one column with
    // the playheads, even though the side groups around the HUD differ in width.
    const auto* pMainWindow = qobject_cast<const QMainWindow*>(window());
    const QWidget* pSkin = pMainWindow && pMainWindow->centralWidget()
            ? pMainWindow->centralWidget()
            : window();
    const int skinCenter =
            mapFromGlobal(pSkin->mapToGlobal(QPoint(pSkin->width() / 2, 0))).x();
    const int half = stackWidth / 2;
    if (width() <= stackWidth) {
        return width() / 2;
    }
    return qBound(half, skinCenter, width() - (stackWidth - half));
}

void WTangoHud::moveEvent(QMoveEvent* pEvent) {
    WWidget::moveEvent(pEvent);
    // The skin center is fixed but the HUD can shift under it (for example
    // when the Auto DJ status light appears), so repaint at the new offset.
    update();
}

QSize WTangoHud::sizeHint() const {
    const QFontMetrics lm(labelFont(font()));
    const QFontMetrics tm(timeFont(font()));
    // Always reserve the pip row so the toolbar height does not jump when a tanda
    // becomes active or idle.
    const QFontMetrics vm(valueFont(font()));
    const int pipRow = mixxx::kTandaProgressPipDiameter + kPipRowGap;
    const int stackH = lm.height() + tm.height() + pipRow;
    // A timing column is a label, a value and the over/under line.
    const int columnH = 2 * lm.height() + vm.height();
    // Reserve both timing columns so the toolbar makes room for them.
    const int columnsW = leftColumnWidth() + rightColumnWidth() + 2 * kColumnGap;
    return QSize(contentWidth() + columnsW,
            qMax(stackH, columnH) + kVerticalPadding);
}

void WTangoHud::paintEvent(QPaintEvent* pEvent) {
    Q_UNUSED(pEvent);
    const int nextKind = static_cast<int>(m_pNextKind->get());
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, true);
    p.setRenderHint(QPainter::TextAntialiasing, true);

    const int h = height();

    // Live state.
    const double seconds = m_pCountdownSeconds->get();
    const int trackCount = static_cast<int>(m_pTandaTrackCount->get());
    const int playingIndex = static_cast<int>(m_pTandaPlayingIndex->get());
    // playingIndex < 0 with a track count means "previewing the upcoming tanda"
    // (a cortina/loose track is active): pips are drawn dimmed.
    const bool preview = playingIndex < 0 && trackCount > 0;

    const QFont lf = labelFont(font());
    const QFont tf = timeFont(font());
    const QFontMetrics lm(lf);
    const QFontMetrics tm(tf);

    const int timeWidth = countdownTimeWidth(tm);
    const int pipD = mixxx::kTandaProgressPipDiameter;
    const int pipGap = mixxx::kTandaProgressPipGap;

    // Settings-panel toggles: hide the countdown timer (label + time) and/or the
    // pips independently. The layout below still reserves every row's space
    // regardless, so the toolbar height never changes when a toggle flips.
    const bool showTimer = m_pShowCountdownTimer->get() > 0.0;
    const bool showPips = m_pShowProgressPips->get() > 0.0;

    // Three centered rows stacked on one vertical axis: label, large time, pips.
    // The pip row is always reserved (see sizeHint) so the stack height is fixed.
    const int labelH = lm.height();
    const int timeH = tm.height();
    const int pipRow = pipD + kPipRowGap;
    const int stackTop = (h - (labelH + timeH + pipRow)) / 2;
    // The stack is as wide as its widest row; center it over the playheads.
    const int stackWidth = contentWidth() - kSidePadding;
    const int centerX = stackCenterX(stackWidth);

    // The set length shows before the set starts, so paint the timing columns
    // before deciding whether there is a countdown to show.
    paintTimingColumns(&p, centerX, stackWidth, stackTop);

    // A manual stop has no countdown to show. An automatic Pause After stop is
    // different: keep its final "Paused / --:--" acknowledgement visible until
    // the DJ resumes or deliberately resets the state.
    if (m_pAutoDJEnabled->get() <= 0.0 && nextKind != 3) {
        return;
    }

    if (showTimer) {
        // --- Label line, centered ----------------------------------------
        p.setFont(lf);
        p.setPen(kColorText);
        p.drawText(QRect(centerX - stackWidth / 2, stackTop, stackWidth, labelH),
                Qt::AlignHCenter | Qt::AlignVCenter,
                countdownLabel(nextKind));

        // --- Time line (large, fixed-width so digits never jitter), centered -
        // In the final 30 s the whole time value breathes red (a smooth faint
        // <-> full pulse) to warn the DJ; otherwise it is the normal white.
        QColor timeColor = kColorText;
        if (inFlashWindow()) {
            const double factor = 0.5 - 0.5 * std::cos(kTwoPi * m_breathPhase);
            const int alpha = kBreathMinAlpha +
                    static_cast<int>(factor * (255 - kBreathMinAlpha));
            timeColor = QColor(kColorAccent.red(),
                    kColorAccent.green(),
                    kColorAccent.blue(),
                    alpha);
        }
        p.setPen(timeColor);
        p.setFont(tf);
        drawFixedWidthTime(&p,
                QRect(centerX - timeWidth / 2, stackTop + labelH, timeWidth, timeH),
                tm,
                formatCountdown(seconds));
    }

    // --- Track pips, centered below the time -----------------------------
    if (showPips && trackCount > 0) {
        const QColor pipColor = preview ? kColorAccentDim : kColorAccent;
        const int pipsWidth = trackCount * pipD + (trackCount - 1) * pipGap;
        int pipX = centerX - pipsWidth / 2;
        const qreal pipTop = stackTop + labelH + timeH + kPipRowGap;
        for (int i = 0; i < trackCount; ++i) {
            QChar state = QLatin1Char('0'); // unplayed
            if (playingIndex >= 0) {
                if (i < playingIndex) {
                    state = QLatin1Char('1'); // played
                } else if (i == playingIndex) {
                    state = QLatin1Char('h'); // playing
                }
            }
            mixxx::drawTandaProgressPip(&p,
                    QRectF(pipX, pipTop, pipD, pipD),
                    pipColor,
                    state);
            pipX += pipD + pipGap;
        }
    }
}

#include "moc_wtangohud.cpp"
