/** @file
 * The opt-in web engine a sketch host shares: explicitly configured, lazy,
 * identical for every borrower, and gone once shut down — leaving the path
 * ready to be configured again.
 */

#include <gtest/gtest.h>
#include <sigilsketch/scry/SharedEngine.h>

#include <utility>

namespace sigil::sketch::scry {

TEST(SketchSharedWebEngine, HostConfigurationIsLazyAndShared) {
  EXPECT_EQ(sharedEngine(), nullptr);

  ::sigil::scry::WebEngineConfig config;
  config.framesPerSecond = 30;
  ASSERT_TRUE(configureSharedEngine(std::move(config)));
  EXPECT_FALSE(configureSharedEngine({}));

  std::shared_ptr<::sigil::scry::WebEngine> first = sharedEngine();
  ASSERT_NE(first, nullptr);
  EXPECT_EQ(sharedEngine(), first);

  // Two sketches may arrive one after the other (and a reload briefly makes
  // them overlap). Both views must therefore come from this engine instead
  // of either sketch attempting a second process-wide renderer.
  std::shared_ptr<::sigil::scry::WebView> firstView = first->createView(32, 32);
  ASSERT_NE(firstView, nullptr);
  std::shared_ptr<::sigil::scry::WebView> secondView =
      sharedEngine()->createView(32, 32);
  ASSERT_NE(secondView, nullptr);

  secondView.reset();
  firstView.reset();
  first.reset();
  shutdownSharedEngine();
  EXPECT_EQ(sharedEngine(), nullptr) << "a shut-down path hands out nothing";

  // …until a host configures it again. The engine that ended is gone and
  // the renderer under it is the process's, so the second configuration
  // is a question that can be answered.
  ASSERT_TRUE(configureSharedEngine({}));
  const std::shared_ptr<::sigil::scry::WebEngine> again = sharedEngine();
  ASSERT_NE(again, nullptr) << "the renderer was not handed back";
  EXPECT_NE(again, first);
  EXPECT_NE(again->createView(32, 32), nullptr);
  shutdownSharedEngine();
}

}  // namespace sigil::sketch::scry
