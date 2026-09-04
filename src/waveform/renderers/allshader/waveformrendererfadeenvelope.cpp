#include "waveform/renderers/allshader/waveformrendererfadeenvelope.h"

#include <QDomNode>
#include <algorithm>
#include <cmath>
#include <vector>

#include "control/controlproxy.h"
#include "skin/legacy/skincontext.h"
#include "waveform/renderers/allshader/matrixforwidgetgeometry.h"
#include "waveform/renderers/waveformwidgetrenderer.h"
#include "widget/wskincolor.h"

namespace {

// Number of segments used to tessellate each ramp. A linear ramp only needs 1,
// but sampling finely keeps the code ready for an eased curve later without
// changing the geometry path.
constexpr int kRampSegments = 32;

// Schematic gain of the envelope at pixel x, given the four knee positions.
// gain ramps 0 -> 1 over [xStart, xPlateauStart], holds at 1 to xPlateauEnd,
// then ramps 1 -> 0 over [xPlateauEnd, xEnd]. Linear for v1.
float envelopeGain(float x,
        float xStart,
        float xPlateauStart,
        float xPlateauEnd,
        float xEnd) {
    float g;
    if (x <= xStart) {
        g = 0.f;
    } else if (x < xPlateauStart) {
        g = (x - xStart) / (xPlateauStart - xStart);
    } else if (x <= xPlateauEnd) {
        g = 1.f;
    } else if (x < xEnd) {
        g = 1.f - (x - xPlateauEnd) / (xEnd - xPlateauEnd);
    } else {
        g = 0.f;
    }
    return std::clamp(g, 0.f, 1.f);
}

} // anonymous namespace

namespace allshader {

WaveformRendererFadeEnvelope::WaveformRendererFadeEnvelope(
        WaveformWidgetRenderer* waveformWidget)
        : WaveformRenderer(waveformWidget) {
}

bool WaveformRendererFadeEnvelope::init() {
    const QString& group = m_waveformRenderer->getGroup();
    m_pShowEnvelopeControl = std::make_unique<ControlProxy>(
            "[TangoQ]", "show_fade_envelope");
    m_pFadeActiveControl = std::make_unique<ControlProxy>(group, "tango_fade_active");
    m_pFadeStartControl =
            std::make_unique<ControlProxy>(group, "tango_fade_start_position");
    m_pFadePlateauStartControl =
            std::make_unique<ControlProxy>(group, "tango_fade_plateau_start_position");
    m_pFadePlateauEndControl =
            std::make_unique<ControlProxy>(group, "tango_fade_plateau_end_position");
    m_pFadeEndControl =
            std::make_unique<ControlProxy>(group, "tango_fade_end_position");
    return true;
}

void WaveformRendererFadeEnvelope::setup(const QDomNode& node, const SkinContext& context) {
    // Sensible translucent defaults; a FadeEnvelopeColor scheme variable can be
    // wired in later (Stage 4 of the plan).
    m_veilColor = QColor(0, 0, 0);
    m_lineColor = QColor(255, 200, 120);

    const QString veilColorName = context.selectString(node, "FadeEnvelopeColor");
    if (!veilColorName.isNull()) {
        m_veilColor = WSkinColor::getCorrectColor(QColor(veilColorName));
    }
    const QString lineColorName = context.selectString(node, "FadeEnvelopeLineColor");
    if (!lineColorName.isNull()) {
        m_lineColor = WSkinColor::getCorrectColor(QColor(lineColorName));
    }
}

void WaveformRendererFadeEnvelope::initializeGL() {
    WaveformRenderer::initializeGL();
    m_shader.init();
}

void WaveformRendererFadeEnvelope::drawEnvelope(
        double xStart, double xPlateauStart, double xPlateauEnd, double xEnd) {
    const float x0 = static_cast<float>(xStart);
    const float x1 = static_cast<float>(xEnd);
    const float xps = static_cast<float>(xPlateauStart);
    const float xpe = static_cast<float>(xPlateauEnd);
    const float span = x1 - x0;
    if (span <= 0.f) {
        return;
    }

    const float breadth = static_cast<float>(m_waveformRenderer->getBreadth());
    const float cy = breadth * 0.5f;
    // Inset the full-gain extent slightly so the plateau reads as a distinct
    // line rather than hugging the very top/bottom edge.
    const float inset = std::max(breadth * 0.06f, 2.0f);
    const float halfH = cy - inset;

    // Sample the envelope at the knees and finely across each ramp, so the
    // shape traces ramp up, flat plateau, ramp down as one continuous outline.
    std::vector<float> xs;
    xs.reserve(2 * kRampSegments + 4);
    xs.push_back(x0);
    for (int i = 1; i < kRampSegments; ++i) {
        xs.push_back(x0 + (xps - x0) * (static_cast<float>(i) / kRampSegments));
    }
    xs.push_back(xps);
    xs.push_back(xpe);
    for (int i = 1; i < kRampSegments; ++i) {
        xs.push_back(xpe + (x1 - xpe) * (static_cast<float>(i) / kRampSegments));
    }
    xs.push_back(x1);

    const int n = static_cast<int>(xs.size());
    std::vector<float> topStrip;
    std::vector<float> bottomStrip;
    std::vector<float> lineTop;
    std::vector<float> lineBottom;
    topStrip.reserve(n * 4);
    bottomStrip.reserve(n * 4);
    lineTop.reserve(n * 2);
    lineBottom.reserve(n * 2);

    for (float x : xs) {
        const float g = envelopeGain(x, x0, xps, xpe, x1);
        const float innerTop = cy - g * halfH;    // envelope boundary, top
        const float innerBottom = cy + g * halfH; // envelope boundary, bottom

        // Top veil: from top edge (y=0) down to the boundary.
        topStrip.push_back(x);
        topStrip.push_back(0.f);
        topStrip.push_back(x);
        topStrip.push_back(innerTop);

        // Bottom veil: from the boundary down to the bottom edge (y=breadth).
        bottomStrip.push_back(x);
        bottomStrip.push_back(innerBottom);
        bottomStrip.push_back(x);
        bottomStrip.push_back(breadth);

        lineTop.push_back(x);
        lineTop.push_back(innerTop);
        lineBottom.push_back(x);
        lineBottom.push_back(innerBottom);
    }

    const int positionLocation = m_shader.positionLocation();
    const int colorLocation = m_shader.colorLocation();

    QColor veil = m_veilColor;
    veil.setAlphaF(0.55f);
    m_shader.setUniformValue(colorLocation, veil);

    m_shader.setAttributeArray(positionLocation, GL_FLOAT, topStrip.data(), 2);
    glDrawArrays(GL_TRIANGLE_STRIP, 0, n * 2);

    m_shader.setAttributeArray(positionLocation, GL_FLOAT, bottomStrip.data(), 2);
    glDrawArrays(GL_TRIANGLE_STRIP, 0, n * 2);

    // Boundary lines make the curve explicit. Drawn as thick ribbons.
    QColor line = m_lineColor;
    line.setAlphaF(0.95f);
    m_shader.setUniformValue(colorLocation, line);

    const float halfWidth = std::max(breadth * 0.008f, 1.0f);
    strokePolyline(lineTop, halfWidth);
    strokePolyline(lineBottom, halfWidth);
}

void WaveformRendererFadeEnvelope::strokePolyline(
        const std::vector<float>& pts, float halfWidth) {
    const int count = static_cast<int>(pts.size() / 2);
    if (count < 2) {
        return;
    }

    std::vector<float> strip;
    strip.reserve(count * 4);
    for (int i = 0; i < count; ++i) {
        const float px = pts[i * 2];
        const float py = pts[i * 2 + 1];
        // Tangent from neighbouring points, so joints stay smooth.
        const int i0 = std::max(i - 1, 0);
        const int i1 = std::min(i + 1, count - 1);
        float tx = pts[i1 * 2] - pts[i0 * 2];
        float ty = pts[i1 * 2 + 1] - pts[i0 * 2 + 1];
        float len = std::sqrt(tx * tx + ty * ty);
        if (len < 1e-6f) {
            tx = 1.f;
            ty = 0.f;
            len = 1.f;
        }
        tx /= len;
        ty /= len;
        const float nx = -ty; // perpendicular
        const float ny = tx;
        strip.push_back(px + nx * halfWidth);
        strip.push_back(py + ny * halfWidth);
        strip.push_back(px - nx * halfWidth);
        strip.push_back(py - ny * halfWidth);
    }

    m_shader.setAttributeArray(m_shader.positionLocation(), GL_FLOAT, strip.data(), 2);
    glDrawArrays(GL_TRIANGLE_STRIP, 0, count * 2);
}

void WaveformRendererFadeEnvelope::paintGL() {
    // Gated off by the Preferences toggle.
    if (!m_pShowEnvelopeControl->toBool()) {
        return;
    }
    // Only draw while AutoDJ publishes a cortina fade for this deck.
    if (!m_pFadeActiveControl->toBool()) {
        return;
    }

    const double trackSamples = m_waveformRenderer->getTrackSamples();
    if (trackSamples <= 0.0) {
        return;
    }

    // The envelope knees are published as playposition (0..1); convert to
    // samples, then to renderer x.
    const double startPosition = m_waveformRenderer->transformSamplePositionInRendererWorld(
            m_pFadeStartControl->get() * trackSamples);
    const double plateauStartPosition =
            m_waveformRenderer->transformSamplePositionInRendererWorld(
                    m_pFadePlateauStartControl->get() * trackSamples);
    const double plateauEndPosition =
            m_waveformRenderer->transformSamplePositionInRendererWorld(
                    m_pFadePlateauEndControl->get() * trackSamples);
    const double endPosition = m_waveformRenderer->transformSamplePositionInRendererWorld(
            m_pFadeEndControl->get() * trackSamples);

    if (endPosition <= startPosition) {
        return;
    }

    // Region not in the current display.
    if (startPosition > m_waveformRenderer->getLength() || endPosition < 0) {
        return;
    }

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    const QMatrix4x4 matrix = matrixForWidgetGeometry(m_waveformRenderer, false);

    const int positionLocation = m_shader.positionLocation();
    const int matrixLocation = m_shader.matrixLocation();

    m_shader.bind();
    m_shader.enableAttributeArray(positionLocation);
    m_shader.setUniformValue(matrixLocation, matrix);

    drawEnvelope(startPosition, plateauStartPosition, plateauEndPosition, endPosition);

    m_shader.disableAttributeArray(positionLocation);
    m_shader.release();
}

} // namespace allshader
