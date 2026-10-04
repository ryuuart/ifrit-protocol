/** @file
 * The handler search every guard in this binary stands on: a catch of a
 * base class matches an exception thrown as a derived one.
 *
 * This binary links the layout engine beneath the composer, which is an
 * archive that throws and catches standard exceptions. One compiled
 * without run-time type information emits a private, non-unique copy of
 * those types' typeinfo, and the linker satisfies every other object's
 * reference to the name with that copy. The search compares a typeinfo
 * by address, so one such archive anywhere in the link leaves every
 * base-class catch in the binary matching nothing the standard library
 * throws; the case below then ends the process rather than failing.
 */

#include <gtest/gtest.h>

#include <stdexcept>
#include <string>

#include "support/CoreTestSupport.h"

namespace sigil::compose {
namespace {

TEST(ComposeStandardExceptions,
     ABaseClassCatchMatchesWhatTheStandardLibraryThrows) {
  const std::string message = "thrown as a derived type";
  bool caught = false;
  try {
    throw std::runtime_error(message);
  } catch (const std::exception& error) {
    caught = true;
    EXPECT_EQ(std::string(error.what()), message);
  }
  EXPECT_TRUE(caught) << "a std::runtime_error is a std::exception, and the "
                         "typeinfo the handler search compares it by is the "
                         "one the standard library threw it with";
}

TEST(ComposeStandardExceptions, AThrowingPaintRestoresTheBorrowedCanvas) {
  Host host;
  host.composer.render(custom([] { throw std::runtime_error("paint failed"); })
                           .width(80)
                           .height(80)
                           .translateX(12)
                           .opacity(0.5f));
  SkCanvas& canvas = *host.surface->getCanvas();
  canvas.translate(3, 7);
  const int saves = canvas.getSaveCount();
  const SkMatrix matrix = canvas.getTotalMatrix();
  const SkIRect clip = canvas.getDeviceClipBounds();

  EXPECT_THROW(host.composer.draw(canvas), std::runtime_error);
  EXPECT_EQ(canvas.getSaveCount(), saves);
  EXPECT_EQ(canvas.getTotalMatrix(), matrix);
  EXPECT_EQ(canvas.getDeviceClipBounds(), clip);

  host.composer.render(box().width(20).height(20).fill(red()));
  EXPECT_NO_THROW(host.composer.draw(canvas));
  EXPECT_EQ(canvas.getSaveCount(), saves);
  EXPECT_EQ(canvas.getTotalMatrix(), matrix);
}

}  // namespace
}  // namespace sigil::compose
