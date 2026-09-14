/** @file
 * The two drives of one settle sequence: the waits a capture is held on,
 * and the readings a window asks a frame at a time.
 *
 * They are written beside each other on purpose. A step is a state of
 * the page — the document is here, the page answers so, the view has
 * stopped painting, a whole painting has landed — and each drive reaches
 * that same state its own way: the blocking one on the door's waits, the
 * arriving one on the readings those waits are made of. A page put
 * through the two therefore stops on the same frame.
 */

#include "sigilsketch/scry/Settling.h"

#include <utility>

namespace sigil::sketch::scry {

namespace {

/** How many repaints a watch or a tail is given before the page is
 *  called broken — the engine's own ticks, and the same bound the
 *  door's waits carry. */
constexpr int kRepaints = 600;

/** How many steps one look may take: every stage of the longest
 *  sequence, so a page that has already met them all arrives in one
 *  rather than in a frame each. */
constexpr int kStages = 8;

}  // namespace

Settling::Settling(sigil::scry::WebView& view, Sequence sequence)
    : m_view(view),
      m_sequence(std::move(sequence)),
      m_events(view),
      m_began(std::chrono::steady_clock::now()) {
  // The events are latched by the member above, before the load below:
  // nothing the engine says about this document can land in the gap
  // between asking for it and listening.
  if (!m_sequence.html.empty())
    m_view.loadHTML(m_sequence.html);
  else if (!m_sequence.url.empty())
    m_view.loadURL(m_sequence.url);
}

Settling::~Settling() = default;

bool Settling::complete() {
  if (finished(m_stage)) return m_stage == Stage::Arrived;
  if (!m_events.awaitLoad()) return stop(false);
  drive();
  if (!m_sequence.question.empty()) {
    const bool there = m_sequence.quiet
                           ? awaitQuiet(m_view, m_events, m_sequence.question,
                                        m_sequence.expected, kRepaints)
                           : awaitAnswer(m_view, m_events, m_sequence.question,
                                         m_sequence.expected, kRepaints);
    if (!there) return stop(false);
  } else if (m_sequence.quiet && !awaitStill()) {
    return stop(false);
  }
  if (m_sequence.whole && !repaintWhole(m_view, m_events)) return stop(false);
  // The page's answer to the script is read last, as the picture is: a
  // caller that asked for one holds a frame and a reply that describe
  // the same document.
  if (m_reply) (void)m_reply.await();
  return stop(true);
}

bool Settling::advance() {
  if (finished(m_stage)) return false;
  for (int taken = 0; taken < kStages && step(); ++taken) {
  }
  return finished(m_stage);
}

bool Settling::arrived() const { return m_stage == Stage::Arrived; }

bool Settling::broken() const { return m_stage == Stage::Broken; }

bool Settling::loaded() const { return m_events.loaded(); }

bool Settling::painted() const { return m_events.painted(); }

std::string Settling::reply() const { return m_reply.text(); }

sigil::scry::WebView::Frame Settling::still() const {
  // ONLY WHEN THE SEQUENCE IS BEHIND IT. Steps along the way accept the
  // frame their own wait would have — the load's, the answer's — and
  // those are not the still: a page driven by a wheel has a picture of
  // itself from before the wheel from the moment it loads, and a body
  // that drew it would show the page the call was made on.
  if (m_stage != Stage::Arrived) return {};
  return m_events.accepted();
}

void Settling::drive() {
  if (!m_sequence.run.empty()) m_reply = Answer(m_view, m_sequence.run);
  if (m_sequence.wheel.x() != 0 || m_sequence.wheel.y() != 0)
    m_view.scroll(m_sequence.wheel.x(), m_sequence.wheel.y());
  if (m_sequence.press) {
    // A page sees no click until the up arrives, which is why one press
    // is three events.
    const int x = m_sequence.press->x(), y = m_sequence.press->y();
    m_view.mouseMove(x, y);
    m_view.mouseDown(x, y);
    m_view.mouseUp(x, y);
  }
}

bool Settling::step() {
  if (finished(m_stage)) return false;
  if (unresponsive()) {
    stop(false);
    return true;
  }
  switch (m_stage) {
    case Stage::Loading:
      if (!m_events.painted()) return false;
      m_events.accept();
      drive();
      m_stage = next(Stage::Loading);
      enter();
      return true;
    case Stage::Watching: {
      if (!m_asked) {
        // A settle that ends on the view going still asks BEFORE it
        // waits, because a page already in the state being asked about
        // will not repaint again to announce it. One that ends on the
        // answer itself asks only after a repaint, so the answer
        // describes the frame standing now.
        if (!((m_sequence.quiet && !m_everAsked) ||
              m_events.repaintsSince(m_mark) > 0))
          return false;
        ask();
        return false;
      }
      if (!m_asked.ready()) return false;
      const bool met = m_asked.text() == m_sequence.expected;
      m_asked = Answer{};
      if (!met) return false;
      // The answer describes the frame standing now, so that frame is
      // the still — unless a tail follows, which ends on a later one.
      if (!m_sequence.quiet) m_events.accept();
      m_stage = next(Stage::Watching);
      enter();
      return true;
    }
    case Stage::Quieting:
      // TWO CONDITIONS, AND THE SECOND IS NOT REDUNDANT. A page whose
      // newest frame is already older than the window is one that
      // stopped painting BEFORE this watch began — the call it is
      // watching for has not been drawn yet — so the window is only
      // quiet once a whole one has passed under the watch itself. The
      // wait this stands for holds its thread for that window every
      // time, which is the same rule said the other way round.
      if (!m_events.quietFor(kQuiet) ||
          std::chrono::steady_clock::now() - m_entered < kQuiet)
        return false;
      m_events.accept();
      m_stage = next(Stage::Quieting);
      enter();
      return true;
    case Stage::Confirming: {
      if (!m_asked) {
        ask();
        return false;
      }
      if (!m_asked.ready()) return false;
      const bool same = m_asked.text() == m_sequence.expected;
      m_asked = Answer{};
      // A page that moved on during the tail never went still, and the
      // frame it stopped on is a picture of a document it has left.
      if (!same) {
        stop(false);
        return true;
      }
      m_stage = next(Stage::Confirming);
      enter();
      return true;
    }
    case Stage::PaintTaller:
      if (m_events.repaintsSince(m_mark) == 0) return false;
      m_stage = Stage::PaintBack;
      enter();
      return true;
    case Stage::PaintBack:
      if (m_events.repaintsSince(m_mark) == 0) return false;
      m_events.accept();
      m_stage = Stage::Arrived;
      return true;
    case Stage::Arrived:
    case Stage::Broken:
      return false;
  }
  return false;
}

Settling::Stage Settling::next(Stage from) const {
  const Stage end = m_sequence.whole ? Stage::PaintTaller : Stage::Arrived;
  switch (from) {
    case Stage::Loading:
      if (!m_sequence.question.empty()) return Stage::Watching;
      return m_sequence.quiet ? Stage::Quieting : end;
    case Stage::Watching:
      return m_sequence.quiet ? Stage::Quieting : end;
    case Stage::Quieting:
      return m_sequence.question.empty() ? end : Stage::Confirming;
    case Stage::Confirming:
      return end;
    case Stage::PaintTaller:
      return Stage::PaintBack;
    default:
      return Stage::Arrived;
  }
}

bool Settling::stop(bool there) {
  m_stage = there ? Stage::Arrived : Stage::Broken;
  return there;
}

void Settling::enter() {
  switch (m_stage) {
    case Stage::Quieting:
      m_entered = std::chrono::steady_clock::now();
      return;
    case Stage::Watching:
      m_mark = m_events.repaints();
      m_everAsked = false;
      return;
    case Stage::PaintTaller:
      // workaround: the engine's own "paint this again" flag marks a
      // view without damaging anything in it, so nothing is repainted
      // and no frame is published. A round trip through a layout
      // viewport one pixel taller damages the whole page twice, and the
      // second painting is at the size and the place the page was
      // already standing.
      m_size = {m_view.width(), m_view.height()};
      m_mark = m_events.repaints();
      m_view.resize(m_size.width(), m_size.height() + 1);
      return;
    case Stage::PaintBack:
      m_mark = m_events.repaints();
      m_view.resize(m_size.width(), m_size.height());
      return;
    default:
      return;
  }
}

bool Settling::awaitStill() {
  for (int tick = 0; tick <= kRepaints; ++tick) {
    // Every frame that still arrives is a later picture of the same
    // document, until a whole quiet window passes without one.
    const uint64_t mark = m_events.repaints();
    if (!m_events.awaitRepaint(mark, kQuiet)) {
      m_events.accept();
      return m_events.painted();
    }
  }
  return false;
}

bool Settling::unresponsive() const {
  return std::chrono::steady_clock::now() - m_began >= kUnresponsive;
}

void Settling::ask() {
  // The mark is taken BEFORE the question, so a repaint that lands while
  // the page is answering is one the next look has already seen.
  m_mark = m_events.repaints();
  m_asked = Answer(m_view, m_sequence.question);
  m_everAsked = true;
}

std::unique_ptr<Settling> settle(sigil::scry::WebView& view, Sequence sequence,
                                 bool deterministic) {
  auto settling = std::make_unique<Settling>(view, std::move(sequence));
  if (deterministic) (void)settling->complete();
  return settling;
}

}  // namespace sigil::sketch::scry
