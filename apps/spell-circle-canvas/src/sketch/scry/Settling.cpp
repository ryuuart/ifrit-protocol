/** @file
 * One settle sequence as one machine, driven two ways: a frame at a time
 * in a window, and blocked through on the thread taking a capture.
 *
 * A step is a state of the page — the document is here, the page answers
 * so, the view has stopped painting, a whole painting has landed, the
 * script has answered — met by a reading of the engine's events. A window
 * takes one look per frame; a capture holds its thread on the event the
 * current step waits for between looks. The looks are the same code, so
 * a page put through the two stops on the same frame.
 */

#include "sigilsketch/scry/Settling.h"

#include <utility>

namespace sigil::sketch::scry {

namespace {

/** How many steps one look may take: every stage of the longest
 *  sequence, so a page that has already met them all arrives in one
 *  look rather than in a frame each. */
constexpr int kStages = 9;

}  // namespace

Settling::Settling(sigil::scry::WebView& view, Sequence sequence)
    : m_view(view),
      m_sequence(std::move(sequence)),
      m_events(view),
      m_moved(std::chrono::steady_clock::now()) {
  // The events are latched by the member above, before the load below:
  // nothing the engine says about this document can land in the gap
  // between asking for it and listening.
  if (!m_sequence.html.empty())
    m_view.loadHTML(m_sequence.html);
  else if (!m_sequence.url.empty())
    m_view.loadURL(m_sequence.url);
}

Settling::~Settling() { restore(); }

bool Settling::complete() {
  // THE SAME LOOKS A WINDOW TAKES, with this thread held between them on
  // what the stage waits for: the page's answer where one is outstanding,
  // otherwise the next repaint or a whole quiet window, whichever comes
  // first — and then the next look.
  while (!finished(m_stage)) {
    if (step()) continue;
    if (m_asked)
      (void)m_asked.await(kQuiet);
    else if (m_stage == Stage::Answering)
      (void)m_reply.await(kQuiet);
    else
      (void)m_events.awaitRepaint(m_events.repaints(), kQuiet);
  }
  return m_stage == Stage::Arrived;
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
  // that drew it would show the page the call was made on. A settle that
  // gave up stopped on the last frame a step accepted — the page as far
  // as it got — so a picture of a failed page is one picture too.
  if (!finished(m_stage)) return {};
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
  // THE STAGE'S OWN CONDITION FIRST, THE DEADLINE SECOND: what the page
  // has done is judged before how long it took, so a page that arrived
  // while nobody was looking is not called late for being looked at late.
  switch (m_stage) {
    case Stage::Loading:
      if (!m_events.painted()) break;
      m_events.accept();
      drive();
      return reach(next(Stage::Loading));
    case Stage::Watching: {
      if (!m_asked) {
        // Asked once per repaint until it answers so. A settle that ends
        // on the view going still asked its first as the stage began,
        // because a page already in the state being asked about will not
        // repaint again to announce it; one that ends on the answer
        // itself asks only after a repaint, so the answer describes the
        // frame standing now.
        if (m_events.repaintsSince(m_mark) == 0) break;
        ask();
        break;
      }
      if (!m_asked.ready()) break;
      const bool met = m_asked.text() == m_sequence.expected;
      m_asked = Answer{};
      if (!met) break;
      // The answer describes the frame standing now, so that frame is
      // the still — unless a tail follows, which ends on a later one.
      if (!m_sequence.quiet) m_events.accept();
      return reach(next(Stage::Watching));
    }
    case Stage::Quieting:
      // TWO CONDITIONS, AND THE SECOND IS NOT REDUNDANT. A page whose
      // newest frame is already older than the window is one that
      // stopped painting BEFORE this watch began — the call it is
      // watching for has not been drawn yet — so the window is only
      // quiet once a whole one has passed under the watch itself.
      if (!m_events.quietFor(kQuiet) ||
          std::chrono::steady_clock::now() - m_entered < kQuiet)
        break;
      m_events.accept();
      return reach(next(Stage::Quieting));
    case Stage::Confirming: {
      if (!m_asked) {
        ask();
        break;
      }
      if (!m_asked.ready()) break;
      const bool same = m_asked.text() == m_sequence.expected;
      m_asked = Answer{};
      // A page that moved on during the tail never went still, and the
      // frame it stopped on is a picture of a document it has left.
      if (!same) {
        stop(false);
        return true;
      }
      return reach(next(Stage::Confirming));
    }
    case Stage::PaintTaller:
      if (m_events.repaintsSince(m_mark) == 0) break;
      return reach(next(Stage::PaintTaller));
    case Stage::PaintBack:
      if (m_events.repaintsSince(m_mark) == 0) break;
      m_events.accept();
      return reach(next(Stage::PaintBack));
    case Stage::Answering:
      // The page's answer to the script is read last, as the picture is:
      // a caller that asked for one holds a frame and a reply that
      // describe the same document.
      if (m_reply && !m_reply.ready()) break;
      return reach(Stage::Arrived);
    case Stage::Arrived:
    case Stage::Broken:
      return false;
  }
  if (!unresponsive()) return false;
  stop(false);
  return true;
}

Settling::Stage Settling::next(Stage from) const {
  // A stage the sequence does not have is skipped by asking what follows
  // it, so the order the stages are declared in is the order a driven
  // page does them.
  switch (from) {
    case Stage::Loading:
      if (!m_sequence.question.empty()) return Stage::Watching;
      return m_sequence.quiet ? Stage::Quieting : next(Stage::Confirming);
    case Stage::Watching:
      return m_sequence.quiet ? Stage::Quieting : next(Stage::Confirming);
    case Stage::Quieting:
      return m_sequence.question.empty() ? next(Stage::Confirming)
                                         : Stage::Confirming;
    case Stage::Confirming:
      return m_sequence.whole ? Stage::PaintTaller : next(Stage::PaintBack);
    case Stage::PaintTaller:
      return Stage::PaintBack;
    case Stage::PaintBack:
      return m_sequence.run.empty() ? Stage::Arrived : Stage::Answering;
    case Stage::Answering:
    case Stage::Arrived:
    case Stage::Broken:
      return Stage::Arrived;
  }
  return Stage::Arrived;
}

bool Settling::reach(Stage to) {
  m_stage = to;
  m_moved = std::chrono::steady_clock::now();
  enter();
  return true;
}

void Settling::stop(bool there) {
  restore();
  m_stage = there ? Stage::Arrived : Stage::Broken;
}

void Settling::enter() {
  switch (m_stage) {
    case Stage::Watching:
      m_mark = m_events.repaints();
      if (m_sequence.quiet) ask();
      return;
    case Stage::Quieting:
      m_entered = std::chrono::steady_clock::now();
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

void Settling::restore() {
  // A view left a pixel taller draws every later frame resampled into
  // the box it stood in, so a whole painting cut short is undone.
  if (m_stage == Stage::PaintTaller)
    m_view.resize(m_size.width(), m_size.height());
}

bool Settling::unresponsive() const {
  return std::chrono::steady_clock::now() - m_moved >= kUnresponsive;
}

void Settling::ask() {
  // The mark is taken BEFORE the question, so a repaint that lands while
  // the page is answering is one the next look has already seen.
  m_mark = m_events.repaints();
  m_asked = Answer(m_view, m_sequence.question);
}

std::unique_ptr<Settling> settle(sigil::scry::WebView& view, Sequence sequence,
                                 bool deterministic) {
  auto settling = std::make_unique<Settling>(view, std::move(sequence));
  if (deterministic) (void)settling->complete();
  return settling;
}

}  // namespace sigil::sketch::scry
