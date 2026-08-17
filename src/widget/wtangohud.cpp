#include "widget/wtangohud.h"

#include <QFontMetrics>
#include <QPainter>
#include <QString>

#include "control/controlproxy.h"
#include "library/autodj/tandaprogresspip.h"
#include "preferences/configobject.h"

namespace {
// Brand palette, matching the TangoQ logo and the rest of the HUD.
const QColor kColorText(0xe8, 0xec, 0xf6);        // white: countdown
const QColor kColorDim(0x8a, 0x93, 0xab);         // grey: inactive tandas
const QColor kColorAccent(0xe6, 0x31, 0x4e);      // red: current tanda + pips
const QColor kColorAccentDim(0xe6, 0x31, 0x4e, 110); // dimmed red: previewed pips

// Horizontal gap between the flow letters and the pip cluster.
constexpr int kFlowPipsGap = 18;
constexpr int kPixelSize = 14;
constexpr int kSidePadding = 20;
constexpr int kCountdownGap = 6;

// The idealized milonga flow. v1 hard-codes this; a configurable pattern in
// Preferences is the next step.
const QString kFlowPattern = QStringLiteral("TTVTTM");

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

QFont hudFont(const QFont& base) {
    QFont f = base;
    f.setPixelSize(kPixelSize);
    f.setBold(true);
    return f;
}
} // namespace

WTangoHud::WTangoHud(QWidget* pParent)
        : WWidget(pParent) {
    const auto makeProxy = [this](const char* key) {
        auto* pProxy = new ControlProxy(
                ConfigKey(kGroup, QString::fromLatin1(key)), this);
        pProxy->connectValueChanged(this, &WTangoHud::slotControlChanged);
        return pProxy;
    };
    m_pCountdownSeconds = makeProxy("hud_countdown_seconds");
    m_pNextKind = makeProxy("hud_next_kind");
    m_pTandaTrackCount = makeProxy("hud_tanda_track_count");
    m_pTandaPlayingIndex = makeProxy("hud_tanda_playing_index");
    m_pFlowIndex = makeProxy("hud_flow_index");
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
    const QFontMetrics fm(hudFont(font()));

    // Row 1: countdown text.
    const int row1Width =
            countdownLabelWidth(fm) + kCountdownGap + countdownTimeWidth(fm);

    // Row 2: flow letters + pips.
    const int letterGap = fm.horizontalAdvance(QChar(' '));
    int flowWidth = 0;
    for (int i = 0; i < kFlowPattern.size(); ++i) {
        flowWidth += fm.horizontalAdvance(kFlowPattern.at(i));
        if (i != kFlowPattern.size() - 1) {
            flowWidth += letterGap;
        }
    }
    const int trackCount = static_cast<int>(m_pTandaTrackCount->get());
    const int pipD = mixxx::kTandaProgressPipDiameter;
    const int pipGap = mixxx::kTandaProgressPipGap;
    const int pipsWidth =
            trackCount > 0 ? trackCount * pipD + (trackCount - 1) * pipGap : 0;
    const int betweenGap = pipsWidth > 0 ? kFlowPipsGap : 0;
    const int row2Width = flowWidth + betweenGap + pipsWidth;

    return qMax(row1Width, row2Width) + kSidePadding;
}

QSize WTangoHud::sizeHint() const {
    return QSize(contentWidth(), 34);
}

void WTangoHud::paintEvent(QPaintEvent* pEvent) {
    Q_UNUSED(pEvent);
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, true);
    p.setRenderHint(QPainter::TextAntialiasing, true);

    const int w = width();
    const int h = height();
    const int rowH = h / 2;

    // Live state.
    const double seconds = m_pCountdownSeconds->get();
    const int nextKind = static_cast<int>(m_pNextKind->get());
    const int trackCount = static_cast<int>(m_pTandaTrackCount->get());
    const int playingIndex = static_cast<int>(m_pTandaPlayingIndex->get());
    const int flowIndex = static_cast<int>(m_pFlowIndex->get());
    // playingIndex < 0 with a track count means "previewing the upcoming tanda"
    // (a cortina/loose track is active): pips are drawn dimmed.
    const bool preview = playingIndex < 0 && trackCount > 0;

    const QFont f = hudFont(font());

    // --- Row 1: countdown ------------------------------------------------
    p.setFont(f);
    p.setPen(kColorText);
    const QFontMetrics row1Metrics(f);
    const int labelWidth = countdownLabelWidth(row1Metrics);
    const int timeWidth = countdownTimeWidth(row1Metrics);
    const int row1Width = labelWidth + kCountdownGap + timeWidth;
    const int row1X = (w - row1Width) / 2;
    p.drawText(QRect(row1X, 0, labelWidth, rowH),
            Qt::AlignRight | Qt::AlignVCenter,
            countdownLabel(nextKind));
    drawFixedWidthTime(&p,
            QRect(row1X + labelWidth + kCountdownGap, 0, timeWidth, rowH),
            row1Metrics,
            formatCountdown(seconds));

    // --- Row 2: T/V/M flow (current red) + track pips --------------------
    const QFontMetrics fm(f);
    const int letterGap = fm.horizontalAdvance(QChar(' '));
    const int flowLen = kFlowPattern.size();
    const int highlight = flowIndex >= 0 && flowLen > 0 ? flowIndex % flowLen : -1;

    int flowWidth = 0;
    for (int i = 0; i < flowLen; ++i) {
        flowWidth += fm.horizontalAdvance(kFlowPattern.at(i));
        if (i != flowLen - 1) {
            flowWidth += letterGap;
        }
    }

    const int pipD = mixxx::kTandaProgressPipDiameter;
    const int pipGap = mixxx::kTandaProgressPipGap;
    const int pipsWidth =
            trackCount > 0 ? trackCount * pipD + (trackCount - 1) * pipGap : 0;
    const int betweenGap = pipsWidth > 0 ? kFlowPipsGap : 0;
    const int totalWidth = flowWidth + betweenGap + pipsWidth;
    const int startX = (w - totalWidth) / 2;
    const int row2Top = rowH;
    const int row2CenterY = row2Top + rowH / 2;

    // Flow letters (current tanda in red).
    int x = startX;
    for (int i = 0; i < flowLen; ++i) {
        p.setPen(i == highlight ? kColorAccent : kColorDim);
        const QChar ch = kFlowPattern.at(i);
        const int advance = fm.horizontalAdvance(ch);
        p.drawText(QRect(x, row2Top, advance, rowH),
                Qt::AlignVCenter | Qt::AlignHCenter,
                QString(ch));
        x += advance + letterGap;
    }

    // Track pips for the current (or, when previewing, the upcoming) tanda.
    const QColor pipColor = preview ? kColorAccentDim : kColorAccent;
    int pipX = startX + flowWidth + betweenGap;
    const qreal pipTop = row2CenterY - pipD / 2.0;
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

#include "moc_wtangohud.cpp"
