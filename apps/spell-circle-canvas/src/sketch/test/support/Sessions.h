#pragma once

/** @file
 * WHAT EVERY SESSION PROMISES, whatever runtime it draws through.
 *
 * A host steps, repaints and photographs a session without ever learning
 * which runtime it is holding, so the five claims below are the whole of
 * what it may rely on — and each runtime would otherwise state them
 * again in its own file, which is how three files came to carry three
 * spellings of one promise. They are stated here once and instantiated
 * per runtime with a traits type declaring
 *
 *     static Kind kind();               // the fixture sketch to open
 *     static SkSize canvas();           // the size its body declares
 *     static double captureSeconds();   // the moment its body declares
 *     static const char* runtime();     // what the kind calls itself
 *     static std::vector<const char*> lanes();  // in the order spent
 *     static float oversample();        // what a still is worth taking at
 *     static void reset();              // zero the body's run count
 *     static int bodyRuns();            // how often the body has run
 *
 * and one line beside it:
 *
 *     INSTANTIATE_TYPED_TEST_SUITE_P(<the runtime>, SessionContract, Traits);
 *
 * The suite is declared at file scope rather than in a namespace because
 * the instantiation macro pastes the suite's name, which a qualified name
 * cannot be pasted onto.
 */

#include <gtest/gtest.h>
#include <include/core/SkCanvas.h>
#include <include/core/SkColor.h>
#include <include/core/SkSurface.h>
#include <sigilsketch/core/Registry.h>
#include <sigilsketch/core/Session.h>

#include <memory>
#include <vector>

#include "support/Fixtures.h"
#include "support/Pixels.h"

/** The session under every claim below, over a raster surface at exactly
 *  the canvas the body declared. */
template <typename Traits>
class SessionContract : public ::testing::Test {
 protected:
  void SetUp() override {
    Traits::reset();
    m_surface = SkSurfaces::Raster(SkImageInfo::MakeN32Premul(
        (int)Traits::canvas().width(), (int)Traits::canvas().height()));
    m_session = Traits::kind()->open(sigil::sketch::test::fonts(),
                                     sigil::sketch::test::assets());
    ASSERT_NE(m_session, nullptr);
  }

  SkCanvas& canvas() { return *m_surface->getCanvas(); }
  sigil::sketch::Session& session() { return *m_session; }

  /** One frame at sixty per second — the rate a host steps at, and the
   *  only one a claim about the seam needs. */
  void step() { m_session->frame(canvas(), 1.0 / 60.0); }

  /** What the surface holds now. */
  SkBitmap plate() { return sigil::sketch::test::plateOf(*m_surface); }

  std::unique_ptr<sigil::sketch::Session> m_session;
  sk_sp<SkSurface> m_surface;
};

TYPED_TEST_SUITE_P(SessionContract);

TYPED_TEST_P(SessionContract, TheCanvasIsWhatTheBodyDeclaredWhileItOpened) {
  // A sketch declares its size from inside its own setup, so a host that
  // read the declaration before opening would letterbox the wrong shape.
  EXPECT_EQ(this->session().canvas().size, TypeParam::canvas());
  EXPECT_EQ(this->session().canvas().captureSeconds,
            TypeParam::captureSeconds());
}

TYPED_TEST_P(SessionContract, TheKindNamesTheRuntimeAndIsOneValuePerSketch) {
  // A kind is a comparable seam value rather than an identity, which is
  // what lets a registry entry be compared with the kind a host holds.
  EXPECT_EQ(TypeParam::kind()->runtime(), TypeParam::runtime());
  EXPECT_TRUE(TypeParam::kind() == TypeParam::kind());
}

TYPED_TEST_P(SessionContract, TheLanesAreTheOnesItsRuntimeNames) {
  this->step();
  const std::vector<const char*> expected = TypeParam::lanes();
  const std::span<const sigil::sketch::Lane> spent = this->session().lanes();
  ASSERT_EQ(spent.size(), expected.size());
  for (size_t i = 0; i < expected.size(); ++i)
    EXPECT_STREQ(spent[i].name, expected[i]) << "lane " << i;
  // A status bar reads this line rather than the lanes, so a runtime
  // that spends lanes and reports no counters leaves one blank.
  EXPECT_FALSE(this->session().counters().empty());
}

TYPED_TEST_P(SessionContract, AFrameIsWhatTheBodyDidPlusWhatTheRuntimeDid) {
  // The seam the performance question always asks about first: a frame
  // splits into the body's own time and the runtime's, and a host that
  // added a third number to the total could not attribute it.
  for (int i = 0; i < 6; ++i) this->step();
  const sigil::sketch::Timing timing = this->session().timing();
  EXPECT_NEAR(timing.totalMs, timing.updateMs + timing.drawMs, 1e-6);
}

TYPED_TEST_P(SessionContract, AStillIsWorthTakingAtTheOversampleItOffers) {
  // A runtime whose drawing is resolution-independent re-renders larger;
  // one whose plate IS the frame it just finished asks for no more.
  EXPECT_FLOAT_EQ(this->session().oversample(), TypeParam::oversample());
}

TYPED_TEST_P(SessionContract, ARepaintDrawsTheStateTheFramesLeftAndNoMore) {
  this->step();
  this->step();
  const int stepped = TypeParam::bodyRuns();
  EXPECT_GT(stepped, 0) << "the body never ran";

  // Two repaints from the same ground are the same picture: a repaint
  // draws the state the frames left and advances nothing.
  this->canvas().clear(SK_ColorBLACK);
  this->session().repaint(this->canvas());
  const SkBitmap first = this->plate();
  this->canvas().clear(SK_ColorBLACK);
  this->session().repaint(this->canvas());
  EXPECT_TRUE(sigil::sketch::test::samePicture(first, this->plate()));
  EXPECT_EQ(TypeParam::bodyRuns(), stepped);
}

REGISTER_TYPED_TEST_SUITE_P(SessionContract,
                            TheCanvasIsWhatTheBodyDeclaredWhileItOpened,
                            TheKindNamesTheRuntimeAndIsOneValuePerSketch,
                            TheLanesAreTheOnesItsRuntimeNames,
                            AFrameIsWhatTheBodyDidPlusWhatTheRuntimeDid,
                            AStillIsWorthTakingAtTheOversampleItOffers,
                            ARepaintDrawsTheStateTheFramesLeftAndNoMore);
