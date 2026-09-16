/** @file
 * The guest with nothing on the other side of it: what a page and a body
 * wearing another application's picture are answered where there is no
 * publication, no recorder and no device. Receiving a real frame wants a
 * second application publishing one, which is the window's lane and not
 * this binary's.
 */

#include <gtest/gtest.h>
#include <sigilsketch/canvas/Guest.h>
#include <sigilsketch/canvas/Sketch.h>

#include "support/Fixtures.h"

namespace {

using sigil::sketch::test::assets;
using sigil::sketch::test::fonts;

/** A context of the shape a body is handed, declaring nothing. */
class GuestContext {
 public:
  explicit GuestContext(bool deterministic)
      : m_composer(m_ticker, fonts()),
        m_context(m_composer, m_ticker, assets(), {0, 0}, &m_specification,
                  &fonts(), deterministic) {}

  sigil::sketch::SketchContext& context() { return m_context; }

 private:
  sigil::motion::Ticker m_ticker;
  sigil::compose::Composer m_composer;
  sigil::sketch::CanvasSpecification m_specification;
  sigil::sketch::SketchContext m_context;
};

TEST(SketchGuest, WithNothingPublishingThereIsNoPictureToWear) {
  GuestContext host(false);
  sigil::sketch::Guest guest(host.context(), "a publication nobody offers");

  // No recorder either: this binary rasterises on the CPU, which is the
  // other half of what a null answer means.
  EXPECT_EQ(guest.frame(nullptr), nullptr);
  EXPECT_FALSE(guest.publishing());
  EXPECT_EQ(guest.name(), "a publication nobody offers");
  EXPECT_TRUE(guest.application().empty());

  // Asking again is what reconnects, so asking twice must answer the
  // same rather than stand on what the first ask left behind.
  EXPECT_EQ(guest.frame(nullptr), nullptr);
  EXPECT_FALSE(guest.publishing());
}

TEST(SketchGuest, WithNothingPublishingThereIsNoTextureToDressABodyWith) {
  GuestContext host(false);
  sigil::sketch::Guest guest(host.context(), "a publication nobody offers");

  // A body's answer is the page's: no picture, and a texture a scene
  // reads as empty rather than one naming nothing.
  EXPECT_FALSE(guest.texture().valid());
  EXPECT_EQ(guest.texture().image(), nullptr);
  EXPECT_FALSE(guest.publishing());

  // The read is what reconnects too, so it answers the same twice.
  EXPECT_FALSE(guest.texture().valid());
}

TEST(SketchGuest, ACaptureThatWillBeDiffedSubscribesToNothing) {
  // Whatever is publishing on the machine a plate is taken on, the
  // picture is the one this sketch declared: a guest opened under the
  // deterministic flag holds no subscription at all.
  GuestContext capture(true);
  sigil::sketch::Guest guest(capture.context(), "Guest");

  EXPECT_EQ(guest.frame(nullptr), nullptr);
  EXPECT_FALSE(guest.texture().valid());
  EXPECT_FALSE(guest.publishing());
  EXPECT_TRUE(guest.application().empty());
}

TEST(SketchGuest, AnUnnamedPublicationIsNoPublication) {
  GuestContext host(false);
  sigil::sketch::Guest guest(host.context(), "");

  EXPECT_EQ(guest.frame(nullptr), nullptr);
  EXPECT_FALSE(guest.texture().valid());
  EXPECT_FALSE(guest.publishing());
  EXPECT_TRUE(guest.name().empty());
}

}  // namespace
