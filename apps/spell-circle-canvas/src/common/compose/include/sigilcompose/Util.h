#pragma once

/** @file
 * SigilCompose util tier — deliberately-demoted sugar a user could
 * write themselves (see DESIGN.md "Kernel and extensions"). Depends
 * only on the kernel + decoration primitives; optional by definition.
 */

#include "sigilcompose/Compose.h"
#include "sigilcompose/Decorations.h"

#include <include/core/SkCanvas.h>
#include <include/core/SkMaskFilter.h>
#include <include/core/SkPaint.h>
#include <include/effects/SkGradient.h>

namespace sigil::compose::util {

/** UTF-8 std::string → std::u8string for text() call sites. */
inline std::u8string toU8(std::string_view s) {
  return std::u8string(s.begin(), s.end());
}

/** Linear gradient Fill — one line over Fill::shader + SkShaders. */
inline Fill linearGradient(SkPoint from, SkPoint to,
                           std::vector<SkColor4f> colors,
                           std::vector<float> stops = {}) {
  SkPoint pts[2] = {from, to};
  return Fill::shader(SkShaders::LinearGradient(
      pts, SkGradient({{colors.data(), colors.size()},
                       {stops.data(), stops.size()},
                       SkTileMode::kClamp},
                      {})));
}

inline Fill radialGradient(SkPoint center, float radius,
                           std::vector<SkColor4f> colors,
                           std::vector<float> stops = {}) {
  return Fill::shader(SkShaders::RadialGradient(
      center, radius,
      SkGradient({{colors.data(), colors.size()},
                  {stops.data(), stops.size()},
                  SkTileMode::kClamp},
                 {})));
}

/** A solid stroke of the node outline (dash/stamp via PathFormat). */
inline PathFormat stroke(float width, Fill fill) {
  PathFormat f;
  f.width = width;
  f.strokeFill = std::move(fill);
  return f;
}

/** A soft drop shadow behind the node's outline — attach with
 *  .background() *before* the fill so the fill paints over it. */
inline Decoration shadow(SkColor4f color, SkVector offset, float blur) {
  return Decoration(PaintProgram(
      [color, offset, blur](SkCanvas &canvas, const PaintContext &ctx) {
        SkPaint p;
        p.setAntiAlias(true);
        p.setColor4f(color, nullptr);
        if (blur > 0)
          p.setMaskFilter(
              SkMaskFilter::MakeBlur(kNormal_SkBlurStyle, blur * 0.5f));
        canvas.save();
        canvas.translate(offset.x(), offset.y());
        canvas.drawPath(ctx.outline, p);
        canvas.restore();
      }));
}

/**
 * The three-line host bundle for the common loop: owns
 * FrameClock + Ticker + Composer, wired together (composer clocked by
 * the FrameClock so pause/time-scale affect everything coherently).
 *
 *   util::Stage stage({1080, 1350}, fontContext);
 *   stage.render(poster(model));          // on data change
 *   bool more = stage.frame(canvas);      // tick + draw + needs-more
 */
class Stage {
public:
  Stage(SkSize size, sigil::weave::FontContext &fonts)
      : m_composer(m_ticker, fonts) {
    m_composer.setClock(&m_clock);
    m_composer.setSize(size);
  }

  tick::FrameClock &clock() { return m_clock; }
  tick::Ticker &ticker() { return m_ticker; }
  Composer &composer() { return m_composer; }

  void render(Element root) { m_composer.render(std::move(root)); }
  void renderSlot(std::string_view name, Element content) {
    m_composer.renderSlot(name, std::move(content));
  }

  /** Ticks real time, draws, and reports whether another frame is
   *  needed (event-driven redraw contract). */
  bool frame(SkCanvas &canvas) {
    const double dt = m_clock.tick();
    const bool animating = m_ticker.tick(dt);
    m_composer.draw(canvas);
    return animating || m_composer.dirty() || m_ticker.active();
  }

private:
  tick::FrameClock m_clock;
  tick::Ticker m_ticker;
  Composer m_composer;
};

} // namespace sigil::compose::util
