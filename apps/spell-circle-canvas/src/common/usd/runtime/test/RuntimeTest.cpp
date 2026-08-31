/** @file
 * usd_runtime_test — the probe answers, and answers the same way twice.
 * Skips, rather than fails, when the USD plugins are absent: that is
 * the condition the probe exists to report.
 */

#include <gtest/gtest.h>
#include <sigilusd/runtime/Runtime.h>

using namespace sigil;

TEST(UsdRuntime, ReportsAvailabilityWithAReason) {
  std::string why;
  const bool ok = usd::runtime::available(&why);
  if (!ok) GTEST_SKIP() << "USD runtime unavailable: " << why;
  EXPECT_TRUE(why.empty()) << "no reason when nothing is missing";
  // Idempotent: the plugin registry is discovered once per process.
  EXPECT_TRUE(usd::runtime::available());
}
