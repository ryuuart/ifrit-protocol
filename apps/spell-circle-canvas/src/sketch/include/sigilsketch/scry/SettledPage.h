#pragma once

/** @file
 * @ingroup sketch-scry
 *
 * Waiting for a web page by the ENGINE'S OWN EVENTS rather than by a
 * stretch of clock — what a deterministic still of a `scry::WebView`
 * needs, and a host concern rather than anything about a look.
 *
 * A CAPTURE WAITS AND A WINDOW NEVER DOES. Everything here is stated
 * twice: as a wait, which a capture drives on the thread taking the
 * still, and as a READING, which answers what the engine has said so far
 * and returns at once. A window holds no thread for a page — it asks the
 * readings once a frame and re-describes as the page arrives — and the
 * sequence both drives is `<sigilsketch/scry/Settling.h>`.
 *
 * A view paints on the engine's thread at the engine's cadence, so a
 * still of a page is a race unless something says when the page is
 * there. THREE ENGINE EVENTS SAY IT. The load callback fires when the
 * main frame has finished loading — the document and everything it
 * pulled in are present. The frame callback fires once per repaint
 * handed over, so counting those counts the pictures of the page. The
 * render-pass callback fires once per pass the engine makes over its
 * pages, published or not, so counting those counts the engine's own
 * ticks — and a stretch with no repaint in it is a number of them. A
 * machine that runs the engine slowly reaches all three later and draws
 * the same picture; a machine that runs it fast reaches them sooner and
 * draws the same picture.
 *
 * WHY THIS IS NOT A DEADLINE. There is one, and it decides nothing about
 * the drawing: it bounds a machine whose engine never loads at all, so a
 * sweep reports instead of hanging. Every machine that gets a page at
 * all settles on the events and never reaches it. A settle rule made of
 * elapsed time is the opposite — it makes how fast the machine ran part
 * of what is drawn.
 *
 * A CALL THAT LANDS OVER SEVERAL FRAMES needs a third thing, because one
 * repaint is not the end of it — the engine walks a wheel smoothly, so
 * the first frame after `scroll` shows the page part of the way there.
 * The document itself is asked instead: a script is dispatched to the
 * page BEFORE the engine's next advance, so an answer read after a
 * repaint describes exactly the state that repaint painted. When the
 * answer is the one asked for, the published frame is the picture of it.
 *
 * AND THE STILL IS THAT FRAME, NOT WHATEVER THE VIEW HOLDS LATER. A view
 * is a live document: it keeps repainting after the settle — a caret, a
 * transition, the tail of a walk — and a leaf that asks the view what it
 * has at PAINT time gets whichever repaint happened to land last, which
 * is a different picture on a machine that ran the engine faster. So the
 * frame the settle accepted is latched here, and the picture is drawn
 * from that.
 */

#include <sigilscry/engine/WebView.h>

#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <memory>
#include <mutex>
#include <string>
#include <string_view>
#include <utility>

namespace sigil::sketch::scry {

/** How long a page that never loads is given before it is called broken.
 *  Not part of the settle rule: nothing about the picture depends on it,
 *  and a machine that reaches it has drawn no page at all. */
inline constexpr std::chrono::seconds kUnresponsive{60};

/** How many passes the engine must make over a view with nothing to
 *  publish before its page is called STILL — see `Events::quietFor`,
 *  which is the only thing that reads it. Long enough that no page which
 *  is still coming is called still, and A COUNT RATHER THAN A STRETCH OF
 *  CLOCK: a loaded machine makes the same passes an idle one does, only
 *  later, so both call the same page still on the same repaint. */
inline constexpr uint64_t kQuietPasses = 60;

/** How long a settle that is BLOCKED holds its thread on one engine
 *  event before it looks again. It decides nothing about the drawing —
 *  every stage's condition is counted, and a look that finds nothing new
 *  simply looks again — it only bounds how promptly a page that has
 *  stopped saying anything reaches the deadline above. */
inline constexpr std::chrono::milliseconds kLook{250};

/** THE SETTLE'S QUIET RULE, AS A VALUE: whether a view that has handed
 *  @p repaints frames over and has seen @p passesSinceRepaint engine
 *  passes since the newest of them has stopped painting. False before
 *  any frame at all, a page that has published nothing being one that
 *  has not finished rather than one at rest.
 *
 *  A COUNT AND NOT A CLOCK, which is the whole of why a still is the
 *  same still on a machine under load: the rule reads what the engine
 *  DID and never how long it took to do it, so the same events in the
 *  same order answer the same way however slowly they arrive. */
[[nodiscard]] inline bool goneQuiet(uint64_t repaints,
                                    uint64_t passesSinceRepaint,
                                    uint64_t window = kQuietPasses) {
  return repaints > 0 && passesSinceRepaint >= window;
}

/**
 * The engine's events for one view, latched.
 *
 * Constructed before the page is loaded, it installs the view's load,
 * frame and render-pass callbacks and is what every stage of driving
 * that page waits on. The callbacks fire on the engine's thread and
 * write through a state block held by shared_ptr, so one still in
 * flight when this object goes away has somewhere valid to write.
 */
class Events {
 public:
  explicit Events(sigil::scry::WebView& view)
      : m_view(&view), m_state(std::make_shared<State>()) {
    auto state = m_state;
    view.setLoadCallback([state] {
      {
        const std::lock_guard<std::mutex> lock(state->mutex);
        state->loaded = true;
        state->repaintsAtLoad = state->repaints;
      }
      state->changed.notify_all();
    });
    view.setFrameCallback([state](const sigil::scry::WebView::Frame& frame) {
      {
        const std::lock_guard<std::mutex> lock(state->mutex);
        ++state->repaints;
        state->latest = frame;
        state->passesAtRepaint = state->passes;
      }
      state->changed.notify_all();
    });
    // COUNTED HERE RATHER THAN TAKEN FROM THE ENGINE: the engine's count
    // is the runtime's and stands wherever the pages before this one left
    // it, while what every reading below asks is how many passes have
    // gone by SINCE something — which is a count that has to start at
    // nothing when this latch does.
    view.setRenderPassCallback([state](uint64_t) {
      {
        const std::lock_guard<std::mutex> lock(state->mutex);
        ++state->passes;
      }
      state->changed.notify_all();
    });
  }

  ~Events() {
    m_view->setLoadCallback({});
    m_view->setFrameCallback({});
    m_view->setRenderPassCallback({});
  }

  Events(const Events&) = delete;
  Events& operator=(const Events&) = delete;

  /** Returns once the main frame has loaded AND a repaint carrying it
   *  has been handed over — the document is here and there is a picture
   *  of it. False only when the engine never got there. */
  [[nodiscard]] bool awaitLoad() const {
    const std::shared_ptr<State> state = m_state;
    std::unique_lock<std::mutex> lock(state->mutex);
    const bool there = state->changed.wait_for(lock, kUnresponsive, [&state] {
      return state->loaded && state->repaints > state->repaintsAtLoad;
    });
    if (there) state->accepted = state->latest;
    return there;
  }

  /** How many repaints have been handed over. Take one before driving
   *  the page and await past it afterwards. */
  [[nodiscard]] uint64_t repaints() const {
    const std::lock_guard<std::mutex> lock(m_state->mutex);
    return m_state->repaints;
  }

  /** Returns once more than @p since repaints have been handed over —
   *  the picture a script, a wheel or a press asked for. @p within is
   *  how long that is waited for, and the default is the deadline that
   *  says a page is broken. Whether the view has stopped painting at all
   *  is a different question and `quietFor` is where it is asked: a
   *  shorter wait here would answer it with how fast the machine ran. */
  [[nodiscard]] bool awaitRepaint(
      uint64_t since, std::chrono::milliseconds within = kUnresponsive) const {
    const std::shared_ptr<State> state = m_state;
    std::unique_lock<std::mutex> lock(state->mutex);
    return state->changed.wait_for(
        lock, within, [&state, since] { return state->repaints > since; });
  }

  /** How many passes the engine has made over this view since the latch
   *  was installed — the engine's own tick, counted whether or not the
   *  page had anything to publish. */
  [[nodiscard]] uint64_t passes() const {
    const std::lock_guard<std::mutex> lock(m_state->mutex);
    return m_state->passes;
  }

  /** How many passes the engine has made since @p mark. */
  [[nodiscard]] uint64_t passesSince(uint64_t mark) const {
    const std::lock_guard<std::mutex> lock(m_state->mutex);
    return m_state->passes > mark ? m_state->passes - mark : 0;
  }

  /** Returns once the engine has made more than @p since passes — THE
   *  TICK EVERY BLOCKED LOOK IS HELD ON, because a pass happens whether
   *  or not the page repaints, so a stage waiting for the page to stop
   *  is woken by the very passes it is counting. @p within bounds the
   *  wait; the default is the deadline that says a page is broken. */
  [[nodiscard]] bool awaitPass(
      uint64_t since, std::chrono::milliseconds within = kUnresponsive) const {
    const std::shared_ptr<State> state = m_state;
    std::unique_lock<std::mutex> lock(state->mutex);
    return state->changed.wait_for(
        lock, within, [&state, since] { return state->passes > since; });
  }

  /** Whether the engine ever said the document arrived. */
  [[nodiscard]] bool loaded() const {
    const std::lock_guard<std::mutex> lock(m_state->mutex);
    return m_state->loaded;
  }

  /** THE READING `awaitLoad` WAITS FOR: the document is here and a
   *  repaint carrying it has been handed over. It answers with whatever
   *  the engine has said by now and never waits, so a window asks it
   *  once a frame; unlike the wait, it accepts no frame — a caller that
   *  stops here calls `accept()`. */
  [[nodiscard]] bool painted() const {
    const std::lock_guard<std::mutex> lock(m_state->mutex);
    return m_state->loaded && m_state->repaints > m_state->repaintsAtLoad;
  }

  /** How many repaints have been handed over since @p mark — none when
   *  the page has not painted since. */
  [[nodiscard]] uint64_t repaintsSince(uint64_t mark) const {
    const std::lock_guard<std::mutex> lock(m_state->mutex);
    return m_state->repaints > mark ? m_state->repaints - mark : 0;
  }

  /** THE READING THE TAIL OF `awaitQuiet` WAITS FOR: a frame has been
   *  handed over and the engine has made @p renderPasses over this view
   *  since, with nothing to publish in any of them, so the view has
   *  stopped painting. False before any frame at all, a page that has
   *  published nothing being one that has not finished rather than one
   *  at rest.
   *
   *  WHAT IS QUIET IS A STRETCH WITH NO EVENT IN IT, and the stretch is
   *  measured in the engine's own passes rather than in elapsed time —
   *  which is what makes the frame a settle stops on the page's answer
   *  instead of the machine's. Load slows the passes down; it does not
   *  change how many of them carried a repaint. */
  [[nodiscard]] bool quietFor(uint64_t renderPasses) const {
    const std::lock_guard<std::mutex> lock(m_state->mutex);
    return goneQuiet(m_state->repaints,
                     m_state->passes - m_state->passesAtRepaint, renderPasses);
  }

  /** KEEPS THE FRAME STANDING NOW as the one the still is of. Called by
   *  every settle that succeeded, so the accepted frame is the one whose
   *  state the settle rule just read. */
  void accept() const {
    const std::lock_guard<std::mutex> lock(m_state->mutex);
    m_state->accepted = m_state->latest;
  }

  /** THE FRAME THE SETTLE ACCEPTED — what a still of this view is a
   *  picture of. Falsy before any settle succeeded.
   *
   *  On a CPU engine it carries the raster image, which is immutable and
   *  therefore exact however long the page goes on repainting. A GPU
   *  engine publishes a texture the view reuses, so `image` is null
   *  there and the caller draws through the view: a GPU still is the
   *  engine's latest either way, and a plate is taken on the CPU tier. */
  [[nodiscard]] sigil::scry::WebView::Frame accepted() const {
    const std::lock_guard<std::mutex> lock(m_state->mutex);
    return m_state->accepted;
  }

 private:
  struct State {
    std::mutex mutex;
    std::condition_variable changed;
    bool loaded = false;
    uint64_t repaints = 0;               // handed over so far
    uint64_t repaintsAtLoad = 0;         // the count when the document arrived
    uint64_t passes = 0;                 // engine passes seen since the latch
    uint64_t passesAtRepaint = 0;        // …and the count at the newest frame
    sigil::scry::WebView::Frame latest;  // the newest handed over
    sigil::scry::WebView::Frame accepted;  // the one a settle stopped on
  };

  sigil::scry::WebView* m_view;
  std::shared_ptr<State> m_state;
};

/**
 * A QUESTION PUT TO THE PAGE, and what it answered — read either way.
 *
 * The engine replies on its own thread, so the reply lands in a state
 * block held by shared_ptr and a question whose asker has gone still has
 * somewhere valid to write. `ready()` says whether it is there without
 * waiting; `await()` is the same answer for a caller that may hold its
 * thread.
 */
class Answer {
 public:
  /** Nothing asked: never ready, and empty. */
  Answer() = default;

  /** Asks @p view for @p expression. */
  Answer(sigil::scry::WebView& view, const std::string& expression)
      : m_state(std::make_shared<State>()) {
    auto state = m_state;
    view.evaluateScript(expression, [state](std::string result) {
      {
        const std::lock_guard<std::mutex> lock(state->mutex);
        state->text = std::move(result);
        state->ready = true;
      }
      state->changed.notify_all();
    });
  }

  /** Whether a question was asked at all. */
  explicit operator bool() const { return m_state != nullptr; }

  /** Whether the engine has replied, WITHOUT WAITING for it. */
  [[nodiscard]] bool ready() const {
    if (!m_state) return false;
    const std::lock_guard<std::mutex> lock(m_state->mutex);
    return m_state->ready;
  }

  /** What the page answered, stringified as `evaluateScript` hands it
   *  back — the exception text where it threw. Empty until ready. */
  [[nodiscard]] std::string text() const {
    if (!m_state) return {};
    const std::lock_guard<std::mutex> lock(m_state->mutex);
    return m_state->text;
  }

  /** Returns once the engine has replied. False only when it never
   *  did, or when nothing was asked. */
  [[nodiscard]] bool await(
      std::chrono::milliseconds within = kUnresponsive) const {
    if (!m_state) return false;
    const std::shared_ptr<State> state = m_state;
    std::unique_lock<std::mutex> lock(state->mutex);
    return state->changed.wait_for(lock, within,
                                   [&state] { return state->ready; });
  }

 private:
  struct State {
    std::mutex mutex;
    std::condition_variable changed;
    bool ready = false;
    std::string text;
  };

  std::shared_ptr<State> m_state;
};

/** What the page answers to @p expression, stringified as
 *  `evaluateScript` hands it back. Empty when the engine never
 *  answered. */
inline std::string answer(sigil::scry::WebView& view,
                          const std::string& expression) {
  const Answer asked(view, expression);
  return asked.await() ? asked.text() : std::string();
}

/**
 * Waits until the page answers @p expected to @p expression, looking
 * once per repaint the engine hands over.
 *
 * The answer is read only after a repaint, and that is what makes it
 * exact: the engine advances the document and then paints it, and a
 * script handed to the view runs before the next advance — so an answer
 * read here describes the state the latest published frame carries.
 * When it is the state that was asked for, that frame is the picture.
 *
 * @p repaints bounds the wait in the engine's own ticks; a page that
 * never gets there returns false rather than spinning.
 */
[[nodiscard]] inline bool awaitAnswer(sigil::scry::WebView& view,
                                      const Events& events,
                                      const std::string& expression,
                                      std::string_view expected,
                                      int repaints = 600) {
  uint64_t mark = events.repaints();
  for (int tick = 0; tick < repaints; ++tick) {
    if (!events.awaitRepaint(mark)) return false;
    mark = events.repaints();
    if (answer(view, expression) == expected) {
      // The answer describes the frame standing now, so that frame is
      // the still — every repaint after it is a later document.
      events.accept();
      return true;
    }
  }
  return false;
}

/**
 * The settle for a page that GOES STILL: it waits for @p expected as
 * above, and then for the view to stop painting. The LAST frame it
 * published is the still.
 *
 * A wheel is the case this exists for. The engine walks a wheel
 * smoothly and the page reports `window.scrollY` as a whole number, so
 * it answers with the position it is heading for while the picture is
 * still a fraction of a pixel short of it — and which frame that answer
 * lands on is decided by how loaded the machine was. The same shape
 * catches a document that has loaded and is still being painted. What
 * ends either is the view going quiet, and the frame it went quiet on
 * is the one to photograph.
 *
 * It asks BEFORE it waits, because a page that has already reached the
 * state being asked about will not repaint again to announce it, and a
 * settle that looked only after the next repaint would wait for
 * something that is not coming. Asking early is harmless here in a way
 * it is not for `awaitAnswer`: whatever frame stood at that moment, the
 * still is the last one of the tail.
 *
 * THE QUIET WINDOW IS COUNTED IN THE ENGINE'S OWN PASSES, so nothing in
 * this header lets a clock decide a drawing: a machine so loaded that
 * the tail of a walk takes twice as long to publish makes the engine's
 * passes twice as slow with it, and the pass the last frame of that walk
 * lands on is the same pass either way.
 *
 * A page that never goes still — a caret, a transition, a loop — wants
 * `awaitAnswer` instead, which stops on the frame its answer describes.
 */
[[nodiscard]] inline bool awaitQuiet(sigil::scry::WebView& view,
                                     const Events& events,
                                     const std::string& expression,
                                     std::string_view expected,
                                     int repaints = 600) {
  bool arrived = false;
  for (int tick = 0; tick <= repaints && !arrived; ++tick) {
    // The mark is taken BEFORE the question, so a repaint that lands
    // while the page is answering is one this loop has already seen.
    const uint64_t mark = events.repaints();
    if (answer(view, expression) == expected) {
      arrived = true;
      break;
    }
    if (!events.awaitRepaint(mark)) return false;
  }
  if (!arrived) return false;
  // The tail: every frame that still arrives is a later picture of the
  // same document, until the engine has made a whole window of passes
  // with none in them.
  for (int tick = 0; tick <= repaints; ++tick) {
    if (events.quietFor(kQuietPasses)) break;
    if (!events.awaitPass(events.passes())) return false;
  }
  events.accept();
  // …and the document the still is of is still the one that was asked
  // for. A page that moved on during the tail never went still.
  return answer(view, expression) == expected;
}

/**
 * Makes @p view hand over a WHOLE fresh painting of the page as it
 * stands, and accepts that as the still.
 *
 * The engine paints what a change damaged and copies the rest, so the
 * picture of a page that has been DRIVEN carries the seams of however
 * the driving happened to be broken into steps: a wheel is walked in
 * several copies and repaints, and where each one's edge fell is a fact
 * about how loaded the machine was, not about the document. It shows up
 * as a curve antialiased two ways along one of those edges — a plate
 * that moves under a busy sweep and holds when its scene is rendered
 * alone. A whole painting has no seams to carry, so a still taken from
 * one is a picture of the document.
 *
 * workaround: the engine's own "paint this again" flag marks a view
 * without damaging anything in it, so nothing is repainted and no frame
 * is published. A round trip through a layout viewport one pixel taller
 * damages the whole page twice, and the second painting is at the size
 * and the place the page was already standing.
 */
[[nodiscard]] inline bool repaintWhole(sigil::scry::WebView& view,
                                       const Events& events) {
  const int width = view.width();
  const int height = view.height();
  const uint64_t taller = events.repaints();
  view.resize(width, height + 1);
  if (!events.awaitRepaint(taller)) {
    // Put back at its size even so: a view left a pixel taller draws
    // every later frame resampled into the box it stood in.
    view.resize(width, height);
    return false;
  }
  const uint64_t back = events.repaints();
  view.resize(width, height);
  if (!events.awaitRepaint(back)) return false;
  events.accept();
  return true;
}

}  // namespace sigil::sketch::scry
