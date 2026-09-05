#pragma once

/** @file
 * Waiting for a web page by the ENGINE'S OWN EVENTS rather than by a
 * stretch of clock — what a deterministic still of a `scry::WebView`
 * needs, and a host concern rather than anything about a look.
 *
 * A view paints on the engine's thread at the engine's cadence, so a
 * still of a page is a race unless something says when the page is
 * there. TWO ENGINE EVENTS SAY IT. The load callback fires when the main
 * frame has finished loading — the document and everything it pulled in
 * are present. The frame callback fires once per repaint handed over, so
 * counting those counts the engine's own ticks. A machine that runs the
 * engine slowly reaches both later and draws the same picture; a machine
 * that runs it fast reaches them sooner and draws the same picture.
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
#include <future>
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

/**
 * The engine's events for one view, latched.
 *
 * Constructed before the page is loaded, it installs the view's load and
 * frame callbacks and is what every stage of driving that page waits on.
 * The callbacks fire on the engine's thread and write through a state
 * block held by shared_ptr, so one still in flight when this object goes
 * away has somewhere valid to write.
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
      }
      state->changed.notify_all();
    });
  }

  ~Events() {
    m_view->setLoadCallback({});
    m_view->setFrameCallback({});
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
   *  the picture a script, a wheel or a press asked for. */
  [[nodiscard]] bool awaitRepaint(uint64_t since) const {
    const std::shared_ptr<State> state = m_state;
    std::unique_lock<std::mutex> lock(state->mutex);
    return state->changed.wait_for(
        lock, kUnresponsive, [&state, since] { return state->repaints > since; });
  }

  /** Whether the engine ever said the document arrived. */
  [[nodiscard]] bool loaded() const {
    const std::lock_guard<std::mutex> lock(m_state->mutex);
    return m_state->loaded;
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
    uint64_t repaints = 0;        // handed over so far
    uint64_t repaintsAtLoad = 0;  // the count when the document arrived
    sigil::scry::WebView::Frame latest;    // the newest handed over
    sigil::scry::WebView::Frame accepted;  // the one a settle stopped on
  };

  sigil::scry::WebView* m_view;
  std::shared_ptr<State> m_state;
};

/** What the page answers to @p expression, stringified as
 *  `evaluateScript` hands it back. Empty when the engine never
 *  answered. */
inline std::string answer(sigil::scry::WebView& view,
                          const std::string& expression) {
  auto answered = std::make_shared<std::promise<std::string>>();
  std::future<std::string> reply = answered->get_future();
  view.evaluateScript(expression, [answered](std::string result) {
    answered->set_value(std::move(result));
  });
  if (reply.wait_for(kUnresponsive) != std::future_status::ready) return {};
  return reply.get();
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

}  // namespace sigil::sketch::scry
