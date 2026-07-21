#pragma once

/** @file
 * SigilCompose util tier — deliberately-demoted sugar a user could
 * write themselves (see DESIGN.md "Kernel and extensions"). Depends
 * only on the kernel + decoration primitives; optional by definition.
 */

#include "sigilcompose/Compose.h"
#include "sigilcompose/Decorations.h"
#include "sigilcompose/Shapes.h"

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

/** A solid stroke of the node outline (dash/stamp via PathFormat).
 *  `align` positions it: Center (default) straddles the outline, Inner
 *  keeps it inside the silhouette, Outer outside (the keyline). */
inline PathFormat stroke(float width, Fill fill,
                         PathFormat::Align align = PathFormat::Align::Center) {
  PathFormat f;
  f.width = width;
  f.strokeFill = std::move(fill);
  f.align = align;
  return f;
}

/** A soft drop shadow behind the node's outline — a value DecorationScheme
 *  (so a static shadowed node prunes without memo). Attach with .background()
 *  *before* the fill so the fill paints over it. */
struct Shadow {
  SkColor4f color = {0, 0, 0, 1};
  SkVector offset = {0, 0};
  float blur = 0;

  /** Bound offsets: when set, the Output's current value REPLACES that
   *  axis of `offset` each paint, and the decoration declares itself
   *  animated (per-frame volatility) — the hover-lift shadow slides
   *  without re-describing. `maxBind` reserves cull reach for the bound
   *  range (bleed() can't read a future value). */
  const choreograph::Output<float> *bindOffsetX = nullptr;
  const choreograph::Output<float> *bindOffsetY = nullptr;
  float maxBind = 0.0f;

  bool operator==(const Shadow &) const = default;
  bool animated() const { return bindOffsetX || bindOffsetY; }
  /** Paint reach beyond the node's bounds (recording cull grows by this) —
   *  the aero-study fix: big soft shadows must not be culled at the node's
   *  picture-cache bounds. */
  float bleed() const {
    return std::max({std::abs(offset.fX), std::abs(offset.fY), maxBind}) +
           blur;
  }

  void paint(SkCanvas &canvas, const PaintContext &ctx) const {
    SkPaint p;
    p.setAntiAlias(true);
    p.setColor4f(color, nullptr);
    if (blur > 0)
      p.setMaskFilter(SkMaskFilter::MakeBlur(kNormal_SkBlurStyle, blur * 0.5f));
    canvas.save();
    canvas.translate(bindOffsetX ? bindOffsetX->value() : offset.x(),
                     bindOffsetY ? bindOffsetY->value() : offset.y());
    canvas.drawPath(ctx.outline, p);
    canvas.restore();
  }
};

inline Shadow shadow(SkColor4f color, SkVector offset, float blur) {
  return Shadow{color, offset, blur};
}

/** The seamless ticker (news crawl, y2k status bar): `content` twice in a
 *  row inside a clipped box, slid by a caller-owned WRAPPING phase Output
 *  in px. Step the phase over [-(w + gap), 0] where w = the content's
 *  width — measure(content, fonts).width() gives it — and the loop is
 *  invisible. Binding translateX is paint-only volatility: the strip's
 *  recording replays every frame, nothing re-records. Keep `content`
 *  keyless (it mounts twice). */
inline Element marquee(Element content,
                       const choreograph::Output<float> *phase,
                       float gap = 0.0f) {
  return box().clip(true).child(box()
                                    .row()
                                    .gap(gap)
                                    .shrink(0)
                                    .alignSelf(Align::Start)
                                    .translateX(phase)
                                    .child(content)
                                    .child(content));
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

  motion::FrameClock &clock() { return m_clock; }
  motion::Ticker &ticker() { return m_ticker; }
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
  motion::FrameClock m_clock;
  motion::Ticker m_ticker;
  Composer m_composer;
};

} // namespace sigil::compose::util
