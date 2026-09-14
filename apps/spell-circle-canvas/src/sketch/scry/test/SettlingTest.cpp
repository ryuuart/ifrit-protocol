/** @file
 * One settle sequence, driven both ways: the readings answer whatever
 * the page has reached and never wait, a sequence advanced a step at a
 * time reaches its end when the engine's events arrive, and the drive a
 * capture takes stops on the frame the door's own waits stop on.
 *
 * One engine at a time stands over the process's renderer, so these are
 * cases ctest runs in processes of their own.
 */

#include <gtest/gtest.h>
#include <include/core/SkBitmap.h>
#include <include/core/SkColor.h>
#include <include/core/SkImage.h>
#include <sigilscry/engine/WebEngine.h>
#include <sigilscry/engine/WebView.h>
#include <sigilsketch/scry/SettledPage.h>
#include <sigilsketch/scry/Settling.h>

#include <chrono>
#include <memory>
#include <string>
#include <thread>

#include "support/Pixels.h"

namespace {

using sigil::scry::WebEngine;
using sigil::scry::WebEngineConfig;
using sigil::scry::WebView;
using sigil::sketch::scry::Sequence;
using sigil::sketch::test::pixelsOf;
using sigil::sketch::test::samePicture;

/** A page that arrives, says so when it is asked to, and then stops: the
 *  script rewrites the heading the settle watches for, and nothing after
 *  that changes the document. */
constexpr const char* kPage = R"(<!doctype html><meta charset="utf-8">
<style>html,body{margin:0;background:#0000ff}
h1{margin:0;padding:8px;color:#fff;font:16px sans-serif}</style>
<h1 id="head">WAITING</h1>)";

/** What the page is put through: the script, the page's own answer that
 *  it landed, the view going still, and a whole painting. */
Sequence sequence() {
  return {.html = kPage,
          .run =
              "document.getElementById('head').textContent = 'SETTLED';"
              "'the heading'",
          .question = "document.getElementById('head').textContent",
          .expected = "SETTLED",
          .quiet = true,
          .whole = true};
}

/** A page taller than any view of it, so a wheel has somewhere to go and
 *  every pixel of the picture says where the walk stopped. */
constexpr const char* kTall = R"HTML(<!doctype html><meta charset="utf-8">
<body style="margin:0;height:1400px;
             background:linear-gradient(#000080,#00ff00)"></body>)HTML";

/** The same page walked down by a wheel: a delta is what the CONTENT
 *  moves by, so down the page is negative. */
Sequence walked() {
  return {.html = kTall,
          .wheel = {0, -120},
          .question = "String(window.scrollY)",
          .expected = "120",
          .quiet = true,
          .whole = true};
}

/** The one engine this process boots. */
std::shared_ptr<WebEngine> engine() {
  static std::shared_ptr<WebEngine> held = WebEngine::create(WebEngineConfig{});
  return held;
}

/** Advances @p settling the way a window's per-frame call does, until it
 *  reports its end — and holds it to the still's rule on the way: the
 *  steps accept frames of their own, and none of them is the still.
 *  False means it never got there. */
bool advanceToTheEnd(sigil::sketch::scry::Settling& settling) {
  using namespace std::chrono_literals;
  bool early = false;
  for (int look = 0; look < 4000; ++look) {
    if (settling.advance()) {
      EXPECT_FALSE(early)
          << "a still stood before the sequence that takes it had ended";
      return true;
    }
    early = early || bool(settling.still());
    std::this_thread::sleep_for(2ms);
  }
  return false;
}

TEST(SketchSettling, TheReadingsAnswerWhateverThePageHasReachedAndNeverWait) {
  const std::shared_ptr<WebEngine> web = engine();
  if (!web) GTEST_SKIP() << "no web engine on this machine";
  const std::shared_ptr<WebView> view = web->createView(120, 90);
  ASSERT_NE(view, nullptr);

  // A view with no document in it: nothing the readings are about has
  // happened, and every one of them says so and comes back. A wait here
  // would hold this case for the whole unresponsive deadline.
  const sigil::sketch::scry::Events events(*view);
  EXPECT_FALSE(events.loaded());
  EXPECT_FALSE(events.painted());
  EXPECT_EQ(events.repaintsSince(events.repaints()), 0u);
  EXPECT_FALSE(sigil::sketch::scry::Answer{}.ready());
  EXPECT_EQ(sigil::sketch::scry::Answer{}.text(), std::string());

  // …and a sequence over that view is at its first step, with no still
  // to draw and no verdict either way.
  sigil::sketch::scry::Settling settling(*view, Sequence{});
  EXPECT_FALSE(settling.advance());
  EXPECT_FALSE(settling.arrived());
  EXPECT_FALSE(settling.broken());
  EXPECT_FALSE(settling.painted());
  EXPECT_FALSE(settling.still());
}

TEST(SketchSettling, ASequenceAdvancedAStepAtATimeArrivesWhenTheEventsDo) {
  const std::shared_ptr<WebEngine> web = engine();
  if (!web) GTEST_SKIP() << "no web engine on this machine";
  const std::shared_ptr<WebView> view = web->createView(160, 120);
  ASSERT_NE(view, nullptr);

  // The drive a window takes: the call that starts it returns at once,
  // with the page still coming.
  const std::unique_ptr<sigil::sketch::scry::Settling> settling =
      sigil::sketch::scry::settle(*view, sequence(), false);
  ASSERT_NE(settling, nullptr);
  EXPECT_FALSE(settling->arrived()) << "the sequence waited for the page";

  ASSERT_TRUE(advanceToTheEnd(*settling)) << "the sequence never finished";
  EXPECT_TRUE(settling->arrived());
  EXPECT_TRUE(settling->loaded());
  EXPECT_TRUE(settling->painted());
  EXPECT_EQ(settling->reply(), "the heading")
      << "the page's answer to the script it was driven with";
  const WebView::Frame still = settling->still();
  ASSERT_TRUE(still.image) << "a CPU engine hands the frame over as an image";
  EXPECT_EQ(pixelsOf(still.image).getColor(0, 0), SK_ColorBLUE);

  // The end is reported ONCE: a body driving this every frame describes
  // itself again on that call and on no other.
  EXPECT_FALSE(settling->advance());
}

TEST(SketchSettling, TheDeterministicDriveStopsOnTheFrameTheWaitsStopOn) {
  const std::shared_ptr<WebEngine> web = engine();
  if (!web) GTEST_SKIP() << "no web engine on this machine";
  const std::shared_ptr<WebView> driven = web->createView(160, 120);
  const std::shared_ptr<WebView> waited = web->createView(160, 120);
  ASSERT_NE(driven, nullptr);
  ASSERT_NE(waited, nullptr);

  // One view through the sequence, driven the way a capture drives it.
  const std::unique_ptr<sigil::sketch::scry::Settling> settling =
      sigil::sketch::scry::settle(*driven, sequence(), true);
  ASSERT_NE(settling, nullptr);
  ASSERT_TRUE(settling->arrived()) << "the capture's drive never arrived";

  // …and the other through the door's own waits, spelled out: the load,
  // the script, the page's answer with the view gone still, and a whole
  // painting.
  const Sequence spelled = sequence();
  const sigil::sketch::scry::Events events(*waited);
  waited->loadHTML(spelled.html);
  ASSERT_TRUE(events.awaitLoad());
  const sigil::sketch::scry::Answer reply(*waited, spelled.run);
  ASSERT_TRUE(sigil::sketch::scry::awaitQuiet(*waited, events, spelled.question,
                                              spelled.expected));
  ASSERT_TRUE(sigil::sketch::scry::repaintWhole(*waited, events));
  ASSERT_TRUE(reply.await());

  EXPECT_EQ(settling->reply(), reply.text());
  ASSERT_TRUE(settling->still().image);
  ASSERT_TRUE(events.accepted().image);
  EXPECT_TRUE(samePicture(pixelsOf(settling->still().image),
                          pixelsOf(events.accepted().image)))
      << "the sequence stopped on a different picture from the waits it "
         "is made of";
}

TEST(SketchSettling, ADrivenPageStopsOnTheSamePictureWhicheverDriveTakesIt) {
  const std::shared_ptr<WebEngine> web = engine();
  if (!web) GTEST_SKIP() << "no web engine on this machine";
  const std::shared_ptr<WebView> held = web->createView(160, 120);
  const std::shared_ptr<WebView> advanced = web->createView(160, 120);
  ASSERT_NE(held, nullptr);
  ASSERT_NE(advanced, nullptr);

  // A WHEEL IS THE CASE THIS EXISTS FOR. The page reports where the walk
  // is HEADING as soon as it is asked, while the picture is still frames
  // behind it, so what says the walk is over is the view going still —
  // and a drive that took a page which had merely not painted YET for a
  // page at rest would stop on the picture from before the wheel.
  const std::unique_ptr<sigil::sketch::scry::Settling> capture =
      sigil::sketch::scry::settle(*held, walked(), true);
  ASSERT_TRUE(capture->arrived()) << "the capture's drive never arrived";

  const std::unique_ptr<sigil::sketch::scry::Settling> window =
      sigil::sketch::scry::settle(*advanced, walked(), false);
  ASSERT_TRUE(advanceToTheEnd(*window)) << "the window's drive never arrived";
  ASSERT_TRUE(window->arrived());

  ASSERT_TRUE(capture->still().image);
  ASSERT_TRUE(window->still().image);
  EXPECT_TRUE(samePicture(pixelsOf(capture->still().image),
                          pixelsOf(window->still().image)))
      << "the two drives stopped on different pictures of one walk";
}

}  // namespace
