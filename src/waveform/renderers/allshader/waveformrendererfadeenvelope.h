#pragma once

#include <QColor>
#include <memory>
#include <vector>

#include "shaders/unicolorshader.h"
#include "util/class.h"
#include "waveform/renderers/allshader/waveformrenderer.h"

class ControlProxy;
class QDomNode;
class SkinContext;

namespace allshader {
class WaveformRendererFadeEnvelope;
}

/// Draws a schematic fade decay envelope over the scrolling waveform.
///
/// This is an indicator, not an audio-accurate gain scope: it draws a clean
/// centre-squeeze veil over the fade region so the waveform visibly pinches
/// toward the centre line as it fades out (and grows out from centre as it
/// fades in).
///
/// Driven by the per-deck tango_fade_* controls published by AutoDJProcessor
/// while a cortina is fading in or out. It draws nothing for stock transition
/// modes, which never publish those controls.
class allshader::WaveformRendererFadeEnvelope final : public allshader::WaveformRenderer {
  public:
    explicit WaveformRendererFadeEnvelope(WaveformWidgetRenderer* waveformWidget);

    void setup(const QDomNode& node, const SkinContext& context) override;

    bool init() override;

    void initializeGL() override;
    void paintGL() override;

  private:
    // Fill the centre-squeeze veil following the piecewise envelope defined by
    // four x positions (in renderer pixels): gain ramps 0 -> 1 over
    // [xStart, xPlateauStart], holds at 1 to xPlateauEnd, then ramps back to 0
    // at xEnd.
    void drawEnvelope(double xStart,
            double xPlateauStart,
            double xPlateauEnd,
            double xEnd);

    // Draws a thick, smooth line along an interleaved (x,y) polyline by building
    // a triangle-strip ribbon, since core-profile GL_LINE width is capped at 1px
    // and unantialiased. Assumes the shader is bound and its colour set.
    void strokePolyline(const std::vector<float>& pts, float halfWidth);

    mixxx::UnicolorShader m_shader;
    std::unique_ptr<ControlProxy> m_pShowEnvelopeControl;
    std::unique_ptr<ControlProxy> m_pFadeActiveControl;
    std::unique_ptr<ControlProxy> m_pFadeStartControl;
    std::unique_ptr<ControlProxy> m_pFadePlateauStartControl;
    std::unique_ptr<ControlProxy> m_pFadePlateauEndControl;
    std::unique_ptr<ControlProxy> m_pFadeEndControl;

    QColor m_veilColor;
    QColor m_lineColor;

    DISALLOW_COPY_AND_ASSIGN(WaveformRendererFadeEnvelope);
};
