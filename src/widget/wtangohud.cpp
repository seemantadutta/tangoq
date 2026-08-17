#include "widget/wtangohud.h"

#include <QFontMetrics>
#include <QPainter>
#include <QString>
#include <QVector>

#include "control/controlproxy.h"
#include "library/autodj/tandaprogresspip.h"
#include "preferences/configobject.h"

namespace {
// Brand palette, matching the TangoQ logo and the rest of the HUD.
const QColor kColorText(0xe8, 0xec, 0xf6);   // white: countdown
const QColor kColorDim(0x8a, 0x93, 0xab);    // grey: inactive tandas
const QColor kColorAccent(0xe6, 0x31, 0x4e); // red: current tanda + pips

// Horizontal gap between the flow letters and the pip cluster.
constexpr int kFlowPipsGap = 18;

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
    m_pNextIsCortina = makeProxy("hud_next_is_cortina");
    m_pTandaTrackCount = makeProxy("hud_tanda_track_count");
    m_pTandaPlayingIndex = makeProxy("hud_tanda_playing_index");
    m_pFlowIndex = makeProxy("hud_flow_index");
}

void WTangoHud::setup(const QDomNode& node, const SkinContext& context) {
    Q_UNUSED(node);
    Q_UNUSED(context);
    // No skin-configurable properties yet; size, object name and connections
    // are handled by commonWidgetSetup().
}

void WTangoHud::slotControlChanged(double value) {
    Q_UNUSED(value);
    update();
}

QSize WTangoHud::sizeHint() const {
    return QSize(240, 34);
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
    const bool nextIsCortina = m_pNextIsCortina->get() > 0.5;
    const int trackCount = static_cast<int>(m_pTandaTrackCount->get());
    const int playingIndex = static_cast<int>(m_pTandaPlayingIndex->get());
    const int flowIndex = static_cast<int>(m_pFlowIndex->get());

    // --- Row 1: countdown ------------------------------------------------
    QFont countdownFont = font();
    countdownFont.setPixelSize(14);
    countdownFont.setBold(true);
    p.setFont(countdownFont);
    p.setPen(kColorText);
    const QString label = nextIsCortina
            ? QStringLiteral("Cortina in  ")
            : QStringLiteral("Next track in  ");
    p.drawText(QRect(0, 0, w, rowH),
            Qt::AlignCenter,
            label + formatCountdown(seconds));

    // --- Row 2: T/V/M flow (current red) + track pips --------------------
    QFont flowFont = font();
    flowFont.setPixelSize(14);
    flowFont.setBold(true);
    const QFontMetrics fm(flowFont);
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
    const int pipsWidth = trackCount > 0
            ? trackCount * pipD + (trackCount - 1) * pipGap
            : 0;

    const int betweenGap = pipsWidth > 0 ? kFlowPipsGap : 0;
    const int totalWidth = flowWidth + betweenGap + pipsWidth;
    const int startX = (w - totalWidth) / 2;
    const int row2Top = rowH;
    const int row2CenterY = row2Top + rowH / 2;

    // Flow letters.
    p.setFont(flowFont);
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

    // Track pips for the current tanda.
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
                kColorAccent,
                state);
        pipX += pipD + pipGap;
    }
}

#include "moc_wtangohud.cpp"
