/** @file
 * The opt-in web engine a sketch host shares: explicitly configured, lazy,
 * identical for every borrower and final once shut down.
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
  EXPECT_EQ(sharedEngine(), nullptr);
  EXPECT_FALSE(configureSharedEngine({}));
}

}  // namespace sigil::sketch::scry
