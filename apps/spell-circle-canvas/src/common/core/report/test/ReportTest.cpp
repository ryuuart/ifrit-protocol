/** @file
 * The reporting door: one format, the standard error by default, and an
 * installed reporter hearing every warning in its place.
 */

#include <gtest/gtest.h>
#include <sigilcore/report/Report.h>

#include <string>
#include <utility>
#include <vector>

namespace {

using sigil::core::installReporter;
using sigil::core::report;
using sigil::core::reportLine;

TEST(CoreReport, LineNamesTheLibraryInBrackets) {
  EXPECT_EQ(reportLine("SigilMotion", "a cue table is short"),
            "[SigilMotion] a cue table is short");
  EXPECT_EQ(reportLine("SigilSketch", "first\nsecond"),
            "[SigilSketch] first\nsecond");
}

TEST(CoreReport, WithoutAReporterTheLineGoesToStandardError) {
  const sigil::core::Reporter previous = installReporter({});
  testing::internal::CaptureStderr();
  report("SigilGeometry", "no device");
  const std::string said = testing::internal::GetCapturedStderr();
  installReporter(previous);
  EXPECT_EQ(said, "[SigilGeometry] no device\n");
}

TEST(CoreReport, AnInstalledReporterHearsInsteadOfStandardError) {
  std::vector<std::pair<std::string, std::string>> heard;
  const sigil::core::Reporter previous =
      installReporter([&](std::string_view library, std::string_view message) {
        heard.emplace_back(library, message);
      });
  testing::internal::CaptureStderr();
  report("SigilData", "a value would not decode");
  const std::string said = testing::internal::GetCapturedStderr();
  const sigil::core::Reporter mine = installReporter(previous);
  EXPECT_TRUE(said.empty());
  ASSERT_EQ(heard.size(), 1u);
  EXPECT_EQ(heard[0].first, "SigilData");
  EXPECT_EQ(heard[0].second, "a value would not decode");
  EXPECT_TRUE(static_cast<bool>(mine));
}

}  // namespace
