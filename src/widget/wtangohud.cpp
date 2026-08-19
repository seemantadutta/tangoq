#include "widget/wtangohud.h"

#include <QFontMetrics>
#include <QPainter>
#include <QString>

#include "control/controlproxy.h"
#include "library/autodj/tandaprogresspip.h"
#include "preferences/configobject.h"

namespace {
// Brand palette, matching the TangoQ logo and the rest of the HUD.
const QColor kColorText(0xe8, 0xec, 0xf6);           // white: countdown
const QColor kColorAccent(0xe6, 0x31, 0x4e);         // red: pips
const QColor kColorAccentDim(0xe6, 0x31, 0x4e, 110); // dimmed red: previewed pips

// The label rides above the time; the time is large so the countdown is hard to
// miss across a dim room. The track pips sit centered below the time, so the
// whole HUD reads as one centered stack rather than an off-axis cluster.
constexpr int kLabelPixelSize = 13;
constexpr int kTimePixelSize = 24;
constexpr int kSidePadding = 20;
// Vertical gap between the time and the pip row.
constexpr int kPipRowGap = 3;
// Breathing room above and below the stacked lines.
constexpr int kVerticalPadding = 4;

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
        return QStringLiteral("Paused after");
    default:
        return QStringLiteral("Next track in");
    }
}

int countdownLabelWidth(const QFontMetrics& fm) {
    return qMax(fm.horizontalAdvance(QStringLiteral("Next track in")),
            qMax(fm.horizontalAdvance(QStringLiteral("Cortina in")),
                    qMax(fm.horizontalAdvance(QStringLiteral("Set ends in")),
                            fm.horizontalAdvance(QStringLiteral("Paused after")))));
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

void drawFixedWidthTime(QPainter* p, const QRect& rect, const QFontMetrics& fm, const QString& time) {
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
}

void WTangoHud::setup(const QDomNode& node, const SkinContext& context) {
    Q_UNUSED(node);
    Q_UNUSED(context);
    // No skin-configurable properties yet; object name and connections are
    // handled by commonWidgetSetup().
}

void WTangoHud::slotControlChanged(double value) {
    Q_UNUSED(value);
    // The content width can change (label text, pip count), so re-query the
    // layout as well as repaint. This keeps the HUD from ever clipping.
    updateGeometry();
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

QSize WTangoHud::sizeHint() const {
    const QFontMetrics lm(labelFont(font()));
    const QFontMetrics tm(timeFont(font()));
    // Always reserve the pip row so the toolbar height does not jump when a tanda
    // becomes active or idle.
    const int pipRow = mixxx::kTandaProgressPipDiameter + kPipRowGap;
    return QSize(contentWidth(),
            lm.height() + tm.height() + pipRow + kVerticalPadding);
}

void WTangoHud::paintEvent(QPaintEvent* pEvent) {
    Q_UNUSED(pEvent);
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, true);
    p.setRenderHint(QPainter::TextAntialiasing, true);

    const int w = width();
    const int h = height();

    // Live state.
    const double seconds = m_pCountdownSeconds->get();
    const int nextKind = static_cast<int>(m_pNextKind->get());
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

    // Three centered rows stacked on one vertical axis: label, large time, pips.
    // The pip row is always reserved (see sizeHint) so the stack height is fixed.
    const int labelH = lm.height();
    const int timeH = tm.height();
    const int pipRow = pipD + kPipRowGap;
    const int stackTop = (h - (labelH + timeH + pipRow)) / 2;

    // --- Label line, centered --------------------------------------------
    p.setFont(lf);
    p.setPen(kColorText);
    p.drawText(QRect(0, stackTop, w, labelH),
            Qt::AlignHCenter | Qt::AlignVCenter,
            countdownLabel(nextKind));

    // --- Time line (large, fixed-width so digits never jitter), centered -
    p.setFont(tf);
    drawFixedWidthTime(&p,
            QRect((w - timeWidth) / 2, stackTop + labelH, timeWidth, timeH),
            tm,
            formatCountdown(seconds));

    // --- Track pips, centered below the time -----------------------------
    if (trackCount > 0) {
        const QColor pipColor = preview ? kColorAccentDim : kColorAccent;
        const int pipsWidth = trackCount * pipD + (trackCount - 1) * pipGap;
        int pipX = (w - pipsWidth) / 2;
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
