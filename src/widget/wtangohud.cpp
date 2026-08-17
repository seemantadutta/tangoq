#include "widget/wtangohud.h"

#include <QFontMetrics>
#include <QPainter>
#include <QVector>

#include "library/autodj/tandaprogresspip.h"

namespace {
// Brand palette, matching the TangoQ logo and the rest of the HUD.
const QColor kColorText(0xe8, 0xec, 0xf6);   // white: countdown
const QColor kColorDim(0x8a, 0x93, 0xab);    // grey: inactive tandas
const QColor kColorAccent(0xe6, 0x31, 0x4e); // red: current tanda + pips

// Horizontal gap between the flow letters and the pip cluster.
constexpr int kFlowPipsGap = 18;
} // namespace

WTangoHud::WTangoHud(QWidget* pParent)
        : WWidget(pParent) {
}

void WTangoHud::setup(const QDomNode& node, const SkinContext& context) {
    Q_UNUSED(node);
    Q_UNUSED(context);
    // No skin-configurable properties yet; size, object name and connections
    // are handled by commonWidgetSetup().
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

    // --- Row 1: countdown ------------------------------------------------
    // STATIC placeholder until AutoDJProcessor data is wired.
    QFont countdownFont = font();
    countdownFont.setPixelSize(14);
    countdownFont.setBold(true);
    p.setFont(countdownFont);
    p.setPen(kColorText);
    p.drawText(QRect(0, 0, w, rowH),
            Qt::AlignCenter,
            QStringLiteral("Next track in  02:30"));

    // --- Row 2: T/V/M flow (current red) + track pips --------------------
    // STATIC placeholder values.
    const QVector<QChar> tandas = {
            QChar('T'), QChar('T'), QChar('V'), QChar('T'), QChar('T'), QChar('M')};
    const int currentTanda = 2; // the Vals
    // Pip states: '1' played, 'h' playing, '0' unplayed.
    const QString pipStates = QStringLiteral("1h00");

    QFont flowFont = font();
    flowFont.setPixelSize(14);
    flowFont.setBold(true);
    const QFontMetrics fm(flowFont);
    const int letterGap = fm.horizontalAdvance(QChar(' '));

    int flowWidth = 0;
    for (int i = 0; i < tandas.size(); ++i) {
        flowWidth += fm.horizontalAdvance(tandas[i]);
        if (i != tandas.size() - 1) {
            flowWidth += letterGap;
        }
    }

    const int pipD = mixxx::kTandaProgressPipDiameter;
    const int pipGap = mixxx::kTandaProgressPipGap;
    const int pipsCount = pipStates.size();
    const int pipsWidth = pipsCount * pipD + (pipsCount - 1) * pipGap;

    const int totalWidth = flowWidth + kFlowPipsGap + pipsWidth;
    int x = (w - totalWidth) / 2;
    const int row2Top = rowH;
    const int row2CenterY = row2Top + rowH / 2;

    // Flow letters.
    p.setFont(flowFont);
    for (int i = 0; i < tandas.size(); ++i) {
        p.setPen(i == currentTanda ? kColorAccent : kColorDim);
        const int advance = fm.horizontalAdvance(tandas[i]);
        p.drawText(QRect(x, row2Top, advance, rowH),
                Qt::AlignVCenter | Qt::AlignHCenter,
                QString(tandas[i]));
        x += advance + letterGap;
    }

    // Track pips.
    int pipX = (w - totalWidth) / 2 + flowWidth + kFlowPipsGap;
    const qreal pipTop = row2CenterY - pipD / 2.0;
    for (const QChar state : pipStates) {
        mixxx::drawTandaProgressPip(&p,
                QRectF(pipX, pipTop, pipD, pipD),
                kColorAccent,
                state);
        pipX += pipD + pipGap;
    }
}

#include "moc_wtangohud.cpp"
