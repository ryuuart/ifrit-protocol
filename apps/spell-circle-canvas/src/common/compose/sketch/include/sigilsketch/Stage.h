#pragma once

/** @file
 * The host loop in one object: a FrameClock, a Ticker and a Composer wired
 * together, for a host whose only job is to tick, draw and ask whether
 * another frame is needed.
 */

#include <include/core/SkSize.h>
#include <sigilcompose/core/Composer.h>
#include <sigilcompose/core/Element.h>
#include <sigilmotion/clock/FrameClock.h>
#include <sigilmotion/clock/Ticker.h>

#include <string_view>
#include <utility>

class SkCanvas;

namespace sigil::weave {
class FontContext;
}

namespace sigil::compose::sketch {

/**
 * The three-line host bundle for the common loop: owns
 * FrameClock + Ticker + Composer, wired together (composer clocked by
 * the FrameClock so pause/time-scale affect everything coherently).
 *
 *   sketch::Stage stage({1080, 1350}, fontContext);
 *   stage.render(poster(model));          // on data change
 *   bool more = stage.frame(canvas);      // tick + draw + needs-more
 */
class Stage {
 public:
  Stage(SkSize size, sigil::weave::FontContext& fonts)
      : m_composer(m_ticker, fonts) {
    m_composer.setClock(&m_clock);
    m_composer.setSize(size);
  }

  motion::FrameClock& clock() { return m_clock; }
  motion::Ticker& ticker() { return m_ticker; }
  Composer& composer() { return m_composer; }

  void render(Element root) { m_composer.render(std::move(root)); }
  void renderSlot(std::string_view name, Element content) {
    m_composer.renderSlot(name, std::move(content));
  }

  /** Ticks real time, draws, and reports whether another frame is
   *  needed (event-driven redraw contract). */
  bool frame(SkCanvas& canvas) {
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

}  // namespace sigil::compose::sketch
