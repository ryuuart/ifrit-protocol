#pragma once

/** @file
 * Waiting on a page, for the two engine test binaries. A view paints on
 * the web thread's own cadence, and a document publishes blank frames
 * before its style lands, so nothing about a page can be read once — it
 * is polled until it settles or the wait is declared expired. Every
 * helper here reports an expired wait as an expired wait, naming what it
 * was waiting for, so a page that never painted never reads as a page
 * that painted the wrong colour.
 */

#include <gtest/gtest.h>
#include <include/core/SkBitmap.h>
#include <include/core/SkColor.h>
#include <include/core/SkImage.h>
#include <include/core/SkImageInfo.h>
#include <sigilscry/engine/WebView.h>

#include <chrono>
#include <cstdint>
#include <cstdio>
#include <functional>
#include <string>
#include <string_view>
#include <thread>

namespace sigil::scry::test {

/** How long a page is given to publish and to paint before a wait is
 *  called expired. Generous, because the answer must come from the page
 *  and not from how loaded the machine is. */
inline constexpr std::chrono::milliseconds kPageWait{15000};

/** A colour as it reads in a failure message. */
inline std::string colourText(SkColor colour) {
  char text[16];
  std::snprintf(text, sizeof(text), "#%08x", colour);
  return text;
}

/** Polls until the view publishes a frame newer than @p sinceVersion. */
inline ::testing::AssertionResult waitForFrame(
    WebView& view, uint64_t sinceVersion,
    std::chrono::milliseconds timeout = kPageWait) {
  const auto deadline = std::chrono::steady_clock::now() + timeout;
  while (std::chrono::steady_clock::now() < deadline) {
    if (view.frameVersion() > sinceVersion)
      return ::testing::AssertionSuccess();
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
  }
  return ::testing::AssertionFailure()
         << "the wait expired: no frame newer than version " << sinceVersion
         << " was ever published";
}

/** Polls @p sample until it answers @p expected. @p what names the thing
 *  waited for, so an expired wait says which one it was. */
inline ::testing::AssertionResult waitForColour(
    std::string_view what, SkColor expected,
    const std::function<SkColor()>& sample,
    std::chrono::milliseconds timeout = kPageWait) {
  const auto deadline = std::chrono::steady_clock::now() + timeout;
  SkColor last = SK_ColorTRANSPARENT;
  while (std::chrono::steady_clock::now() < deadline) {
    last = sample();
    if (last == expected) return ::testing::AssertionSuccess();
    std::this_thread::sleep_for(std::chrono::milliseconds(20));
  }
  return ::testing::AssertionFailure()
         << "the wait for " << what << " expired: wanted "
         << colourText(expected) << ", last saw " << colourText(last);
}

/** The centre pixel of the view's latest published frame. Transparent
 *  while no frame carries an image, which is every frame of an engine
 *  that publishes a texture instead. */
inline SkColor frameCentre(WebView& view) {
  const WebView::Frame frame = view.frame();
  if (!frame.image) return SK_ColorTRANSPARENT;
  SkBitmap pixels;
  if (!pixels.tryAllocPixels(SkImageInfo::MakeN32Premul(frame.image->width(),
                                                        frame.image->height())))
    return SK_ColorTRANSPARENT;
  if (!frame.image->readPixels(nullptr, pixels.pixmap(), 0, 0))
    return SK_ColorTRANSPARENT;
  return pixels.getColor(frame.image->width() / 2, frame.image->height() / 2);
}

/** Polls the centre of the view's frames until it is @p expected. */
inline ::testing::AssertionResult waitForCentre(
    WebView& view, SkColor expected,
    std::chrono::milliseconds timeout = kPageWait) {
  return waitForColour(
      "the centre of the page", expected, [&view] { return frameCentre(view); },
      timeout);
}

}  // namespace sigil::scry::test
