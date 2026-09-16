#pragma once

/** @file
 * A PAGE'S SETTLE AS ONE SEQUENCE, driven either way: blocked through on
 * the thread that takes a capture, or advanced a step per frame in a
 * window that must keep drawing while the page is still coming.
 *
 * The steps are the ones a still of a driven page needs — load, an
 * optional script, an optional wheel or press, the page's own answer
 * that the driving landed, the view going quiet, a whole repaint, and
 * the script's own answer — and they are one machine whichever way it is
 * driven: a window takes one look per frame, and a capture holds its
 * thread on the event the current step waits for between looks.
 * `<sigilsketch/scry/SettledPage.h>` is the door underneath: every
 * reading a look asks and every wait a capture is held on belongs to the
 * engine's own events.
 *
 * WHICH THREAD DOES WHAT. The engine paints and answers on its own
 * thread and writes what it did into the latched events. Everything
 * here — the constructor's load, `complete`, `advance`, and the calls
 * they make into the view — runs on the CALLER'S thread, which is a
 * sketch's setup thread and then the thread its frames are drawn on, and
 * it is never the engine's. `complete` holds that thread until the page
 * is there, which is what a capture wants and what a window must never
 * do; `advance` returns at once whatever the page has reached, which is
 * what a window asks once a frame.
 */

#include <include/core/SkPoint.h>
#include <include/core/SkSize.h>
#include <sigilscry/engine/WebView.h>
#include <sigilsketch/scry/SettledPage.h>

#include <chrono>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>

namespace sigil::sketch::scry {

/**
 * WHAT A PAGE IS PUT THROUGH before it is a picture: a document, what is
 * done to it, and what says that landed.
 *
 * Every field past the document is optional, and what is left out is a
 * step the sequence does not have: a page that is only displayed
 * declares its document alone and is there when the document is.
 */
struct Sequence {
  /** The document, as markup. */
  std::string html;
  /** …or as an address, for a page this process does not carry. A
   *  sequence names one of the two. */
  std::string url;
  /** JavaScript evaluated once the document is here; what it evaluated
   *  to is the settle's `reply()`. Empty asks nothing. */
  std::string run;
  /** Wheel delivered once the document is here, in pixels of delta —
   *  WHAT THE CONTENT MOVES BY, so a negative y walks down the page.
   *  Zero delivers none. */
  SkIPoint wheel{0, 0};
  /** Where a press lands, in view pixels: a move, a down and an up, so
   *  a page that listens for the up sees a click. */
  std::optional<SkIPoint> press;
  /** What the page is asked, and the answer that says the driving
   *  landed — the DOCUMENT'S own statement about itself rather than a
   *  count of frames. Asked once per repaint until it answers so. Empty
   *  asks nothing, and then nothing but the load says the page is
   *  there. */
  std::string question;
  std::string expected;
  /** Whether the settle ends on the view going STILL: every frame that
   *  still arrives is a later picture of the same document, and the one
   *  it goes quiet on is the still. A call that lands over several
   *  frames — a wheel, a transition — needs this; a page that never
   *  stops painting must not ask for it. */
  bool quiet = false;
  /** Whether to end by making the page paint WHOLE. The engine paints
   *  what a change damaged and copies the rest, so a still of a page
   *  that was driven carries the seams of however the driving was broken
   *  into steps unless it is taken from a whole painting. */
  bool whole = false;
};

/**
 * ONE PAGE'S SETTLE, RUNNING.
 *
 * Constructing it latches the view's events and loads the document, so
 * nothing the engine says can land in the gap between asking and
 * listening. From there the sequence is driven one of two ways and the
 * end is the same either way: the frame the settle stopped on is
 * `still()`, and every frame the view publishes after it is a later
 * document.
 *
 * It holds the view by reference and the view must outlive it, which is
 * what a sketch holding both as members gets for free.
 */
class Settling {
 public:
  Settling(sigil::scry::WebView& view, Sequence sequence);
  ~Settling();

  Settling(const Settling&) = delete;
  Settling& operator=(const Settling&) = delete;

  /** DRIVES THE WHOLE SEQUENCE HERE AND NOW, from wherever it stands,
   *  holding this thread on the engine's events until the page is
   *  there or given up. What a capture calls, and what nothing on a
   *  render thread may. True when the page arrived. */
  bool complete();

  /** ONE LOOK AT WHAT THE ENGINE HAS SAID, and the next step where the
   *  current one is met. It waits for nothing.
   *
   *  True exactly on the call the sequence FINISHES on — arrived or
   *  broken — which is when a sketch describes itself again; false on
   *  every call before and after, so a body may drive it from `update`
   *  every frame and re-describe on nothing else. */
  bool advance();

  /** Whether the whole sequence is behind it. */
  [[nodiscard]] bool arrived() const;
  /** Whether it gave up: a document that never came, or a page that
   *  moved on from the state it had been asked about. */
  [[nodiscard]] bool broken() const;
  /** Whether the load callback has fired — the document is here,
   *  whatever has been painted of it. */
  [[nodiscard]] bool loaded() const;
  /** Whether a repaint carrying the loaded document has been handed
   *  over: there is a picture of the page. */
  [[nodiscard]] bool painted() const;

  /** What `Sequence::run` evaluated to, stringified as the engine hands
   *  it back. Empty until the page has answered, and where nothing was
   *  asked. */
  [[nodiscard]] std::string reply() const;

  /** THE FRAME THE SETTLE STOPPED ON — what a still of this page is a
   *  picture of. Falsy until the sequence is behind it, because a frame
   *  accepted part of the way through is a picture of the page before
   *  the call it is being driven with landed; a sketch draws the view's
   *  own latest until there is one. A settle that gave up stopped on the
   *  last frame a step accepted — the page as far as it got — so a
   *  picture of a failed page is one picture too, and falsy only where
   *  no step accepted any. */
  [[nodiscard]] sigil::scry::WebView::Frame still() const;

 private:
  /** Where the sequence stands. Each is a thing the page has yet to do,
   *  in the order a driven page does them. */
  enum class Stage {
    Loading,      // the document and a repaint carrying it
    Watching,     // the page's own answer that the driving landed
    Quieting,     // every later frame, until the engine's passes run dry
    Confirming,   // …and the document is still the one that was asked for
    PaintTaller,  // a whole painting: one pixel taller…
    PaintBack,    // …and back at the size the page was standing at
    Answering,    // the script's own answer, where one was asked for
    Arrived,
    Broken,
  };

  /** The script, the wheel and the press, dispatched once the document
   *  is here. */
  void drive();
  /** ONE LOOK: the stage now standing is met and the sequence moves on,
   *  or it is not, and a stage that has stood longer than a page that
   *  is coming at all gives the page up. True when the stage moved. */
  bool step();
  /** The stage a met @p from leads to, given what the sequence has. */
  [[nodiscard]] Stage next(Stage from) const;
  /** Moves the sequence on to @p to, entering it. Always true, so a
   *  look that moved says so in one word. */
  bool reach(Stage to);
  /** Ends the sequence. The frame a still is of was accepted by the
   *  step that stopped on it, so nothing is latched here; a view left
   *  taller by a whole painting cut short is put back. */
  void stop(bool there);
  /** What entering the stage now standing does: the mark its readings
   *  are measured from, the first question of a settle that ends on the
   *  view going still, and the resize a whole painting is made by. */
  void enter();
  /** Puts the view back at its own size where a whole painting was cut
   *  short between its two paintings. */
  void restore();
  [[nodiscard]] static bool finished(Stage stage) {
    return stage == Stage::Arrived || stage == Stage::Broken;
  }
  /** Whether the stage now standing has stood longer than a page that
   *  is coming at all takes to meet it. Counted from the last move, so
   *  a page that arrived while nobody was looking is not late for
   *  being looked at late. */
  [[nodiscard]] bool unresponsive() const;
  /** Asks the sequence's question, marking the repaint count first, so
   *  a frame that lands while the page is answering is one the next
   *  look has already seen. */
  void ask();

  sigil::scry::WebView& m_view;
  Sequence m_sequence;
  Events m_events;
  Answer m_reply;  // what `run` evaluated to
  Answer m_asked;  // the question outstanding, where one is
  uint64_t m_mark =
      0;  // the repaint count the outstanding question was asked at
  std::chrono::steady_clock::time_point m_moved;  // when the stage last did
  uint64_t m_entered = 0;  // the engine's pass count a quiet watch began at
  SkISize m_size{0, 0};    // the view's own pixels, while it paints whole
  Stage m_stage = Stage::Loading;
};

/**
 * @p view LOADED AND SETTLED THROUGH @p sequence, driven the way
 * @p deterministic says.
 *
 * A DETERMINISTIC SESSION IS A CAPTURE and its picture is diffed, so the
 * page has to be there before the frame is: the sequence is blocked
 * through here and what comes back has arrived or given up. A live
 * session is a window, and nothing may hold the thread it draws on: the
 * sequence comes back at its first step and the body advances it from
 * its own per-frame call, describing itself again when it finishes.
 */
[[nodiscard]] std::unique_ptr<Settling> settle(sigil::scry::WebView& view,
                                               Sequence sequence,
                                               bool deterministic);

}  // namespace sigil::sketch::scry
