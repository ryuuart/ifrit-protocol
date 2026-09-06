/** @file
 * A claim and the sentence it prints, one case per kind of claim, so the
 * verdict a reader sees and the count a build reads cannot disagree.
 */

#include <gtest/gtest.h>
#include <sigilmeasure/check/Check.h>

#include <string>
#include <string_view>
#include <vector>

using namespace sigil::measure;

TEST(Check, AnEqualIntegralPairPassesAndAnUnequalOneDoesNot) {
  EXPECT_TRUE(check("pieces", 12, 12).pass);
  EXPECT_FALSE(check("pieces", 12, 11L).pass);
}

TEST(Check, AToleranceBandIsPrintedBesideTheValueItAllows) {
  const Check ok = check("radius", 257.972, 257.9725, 0.001);
  EXPECT_TRUE(ok.pass);
  EXPECT_EQ(ok.expected, "257.972 \xc2\xb1 0.001");
  EXPECT_FALSE(check("radius", 1.0, 1.5, 0.25).pass);
}

TEST(Check, TextIdentityIsAByteComparison) {
  EXPECT_TRUE(check("name", std::string_view("a"), std::string_view("a")).pass);
  EXPECT_FALSE(
      check("name", std::string_view("a"), std::string_view("b")).pass);
}

TEST(Check, ABareConditionIsReportedAgainstTrue) {
  const Check b = check("closed", false);
  EXPECT_EQ(b.actual, "false");
  EXPECT_EQ(b.expected, "true");
  EXPECT_FALSE(b.pass);
}

namespace {
/** One printed line: the claim behind it, the two column widths it is
 *  asked for, and the string it must produce. */
struct Row {
  const char* name;
  Check made;
  int labelWidth, valueWidth;
  const char* line;
};

std::string rowName(const testing::TestParamInfo<Row>& info) {
  return info.param.name;
}

struct Lines : testing::TestWithParam<Row> {};
}  // namespace

TEST_P(Lines, PadTheLabelAndRightAlignTheValueAndSayWhatWasWanted) {
  const Row& row = GetParam();
  EXPECT_EQ(row.made.line(row.labelWidth, row.valueWidth), row.line);
}

INSTANTIATE_TEST_SUITE_P(
    Formatting, Lines,
    testing::Values(
        Row{"integral", check("pieces", 12, 12), 10, 4,
            "  pieces       12   PASS"},
        Row{"integralFailure", check("pieces", 12, 11L), 10, 4,
            "  pieces       11   FAIL want 12"},
        Row{"tolerance", check("radius", 1.0, 1.5, 0.25), 8, 4,
            "  radius    1.5   FAIL want 1 \xc2\xb1 0.25"},
        Row{"text", check("name", std::string_view("a"), std::string_view("a")),
            10, 4, "  name          a   PASS"},
        Row{"condition", check("closed", false), 10, 4,
            "  closed     false   FAIL want true"},
        // A long label pushes the value column right rather than losing
        // the units or the qualifier at the end of a claim.
        Row{"longLabel", check("a label longer than its column", 1, 1), 4, 2,
            "  a label longer than its column  1   PASS"}),
    rowName);

TEST(Check, FailuresCountsAndTableSummarises) {
  Table t;
  t.add(check("a", 1, 1)).add(check("b", 1, 2)).add(check("c", true));
  EXPECT_EQ(t.failures(), 1);
  EXPECT_FALSE(t.pass());
  EXPECT_EQ(failures(t.rows), 1);
  const std::vector<std::string> lines = t.lines(4, 2);
  ASSERT_EQ(lines.size(), 4u);
  EXPECT_EQ(lines[0], "  a     1   PASS");
  EXPECT_EQ(lines[1], "  b     2   FAIL want 1");
  EXPECT_EQ(lines[2], "  c    true   PASS");
  EXPECT_EQ(lines[3], "  3 checks, 1 failed");
  Table all;
  all.add(check("x", 2, 2));
  EXPECT_EQ(all.lines().back(), "  1 checks, all passed");
  EXPECT_TRUE(Table{}.lines().empty());
}

TEST(Check, AFindingIsPrintedAsAClaimAndNeverCountedAgainstTheRun) {
  const Check legend = finding(check("legend holds", 1.0, 1.126, 0.01));
  EXPECT_FALSE(legend.pass);
  EXPECT_EQ(legend.standing, Standing::Finding);
  EXPECT_EQ(legend.line(12, 5),
            "  legend holds 1.126   FAIL want 1 \xc2\xb1 0.01");
  Table t;
  t.add(check("a", 1, 1)).add(legend);
  EXPECT_EQ(t.failures(), 0);
  EXPECT_EQ(t.findings(), 1);
  EXPECT_TRUE(t.pass());
  EXPECT_EQ(t.lines(4, 2).back(), "  2 checks, all passed, 1 finding");
  // A finding that holds is a finding of nothing.
  t.add(finding(check("b", 2, 2)));
  EXPECT_EQ(t.findings(), 1);
}

TEST(Check, ReadingsAndHeadingsStandBesideTheClaimsUnjudged) {
  const Check residual = reading("max residual", 5.6e-16);
  EXPECT_TRUE(residual.pass);
  EXPECT_FALSE(residual.judged());
  EXPECT_EQ(residual.line(12, 8), "  max residual  5.6e-16");
  EXPECT_EQ(reading("pieces", 12).line(6, 2), "  pieces 12");
  EXPECT_EQ(reading("centre", "305.185, 393.529").line(6, 2),
            "  centre 305.185, 393.529");
  const Check title = heading("THE RETE IS ONE PIECE OF METAL");
  EXPECT_EQ(title.line(), "THE RETE IS ONE PIECE OF METAL");
  EXPECT_EQ(title.standing, Standing::Heading);
  // Neither is a check: the summary counts the claims alone.
  Table t;
  t.add(title).add(residual).add(check("spurs", 0, 0)).add(reading("bars", 41));
  EXPECT_EQ(t.checks(), 1);
  EXPECT_EQ(t.failures(), 0);
  const std::vector<std::string> lines = t.lines(6, 2);
  ASSERT_EQ(lines.size(), 5u);
  EXPECT_EQ(lines[0], "THE RETE IS ONE PIECE OF METAL");
  EXPECT_EQ(lines[2], "  spurs   0   PASS");
  EXPECT_EQ(lines[3], "  bars   41");
  EXPECT_EQ(lines[4], "  1 checks, all passed");
}
