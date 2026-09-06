#include <gtest/gtest.h>
#include <sigilmeasure/Measure.h>

#include <chrono>
#include <cmath>
#include <string>
#include <string_view>
#include <thread>
#include <type_traits>
#include <utility>
#include <vector>

using namespace sigil::measure;

// ---------------------------------------------------------------------------
// quantile

TEST(Quantile, EmptyReadsZero) { EXPECT_DOUBLE_EQ(quantile({}, 0.5), 0.0); }

TEST(Quantile, OneSampleReadsItselfEverywhere) {
  const double one[] = {7.5};
  EXPECT_DOUBLE_EQ(quantile(one, 0.0), 7.5);
  EXPECT_DOUBLE_EQ(quantile(one, 0.5), 7.5);
  EXPECT_DOUBLE_EQ(quantile(one, 1.0), 7.5);
}

TEST(Quantile, InterpolatesBetweenRanks) {
  const double four[] = {4, 1, 3, 2};  // unsorted on purpose
  EXPECT_DOUBLE_EQ(quantile(four, 0.0), 1.0);
  EXPECT_DOUBLE_EQ(quantile(four, 0.5), 2.5);
  EXPECT_DOUBLE_EQ(quantile(four, 1.0), 4.0);
  EXPECT_DOUBLE_EQ(quantile(four, 1.0 / 3.0), 2.0);
  EXPECT_NEAR(quantile(four, 0.99), 3.97, 1e-9);
}

TEST(Quantile, ClampsTheFraction) {
  const double two[] = {1, 2};
  EXPECT_DOUBLE_EQ(quantile(two, -1.0), 1.0);
  EXPECT_DOUBLE_EQ(quantile(two, 2.0), 2.0);
}

TEST(Quantile, LeavesTheInputAlone) {
  std::vector<double> v = {3, 1, 2};
  (void)quantile(v, 0.5);
  EXPECT_EQ(v, (std::vector<double>{3, 1, 2}));
}

// ---------------------------------------------------------------------------
// Samples

TEST(Samples, EmptyReadsZeroEverywhere) {
  Samples w(4);
  EXPECT_TRUE(w.empty());
  EXPECT_EQ(w.size(), 0u);
  EXPECT_DOUBLE_EQ(w.mean(), 0.0);
  EXPECT_DOUBLE_EQ(w.percentile(0.99), 0.0);
  EXPECT_DOUBLE_EQ(w.min(), 0.0);
  EXPECT_DOUBLE_EQ(w.max(), 0.0);
  EXPECT_DOUBLE_EQ(w.last(), 0.0);
}

TEST(Samples, WrapsAroundKeepingTheNewest) {
  Samples w(3);
  for (double v : {1.0, 2.0, 3.0, 4.0, 5.0}) w.add(v);
  EXPECT_EQ(w.size(), 3u);
  EXPECT_EQ(w.capacity(), 3u);
  EXPECT_EQ(w.samples(), (std::vector<double>{3, 4, 5}));
  EXPECT_DOUBLE_EQ(w.mean(), 4.0);
  EXPECT_DOUBLE_EQ(w.min(), 3.0);
  EXPECT_DOUBLE_EQ(w.max(), 5.0);
  EXPECT_DOUBLE_EQ(w.last(), 5.0);
  EXPECT_DOUBLE_EQ(w.percentile(0.5), 4.0);
}

TEST(Samples, OrderSurvivesExactlyOneLap) {
  Samples w(3);
  for (double v : {1.0, 2.0, 3.0}) w.add(v);
  EXPECT_EQ(w.samples(), (std::vector<double>{1, 2, 3}));
  w.add(4.0);
  EXPECT_EQ(w.samples(), (std::vector<double>{2, 3, 4}));
  EXPECT_DOUBLE_EQ(w.last(), 4.0);
}

TEST(Samples, ClearForgetsButKeepsCapacity) {
  Samples w(2);
  w.add(1.0);
  w.clear();
  EXPECT_TRUE(w.empty());
  EXPECT_EQ(w.capacity(), 2u);
  w.add(9.0);
  EXPECT_DOUBLE_EQ(w.last(), 9.0);
}

TEST(Samples, ZeroCapacityHoldsOne) {
  Samples w(0);
  w.add(1.0);
  w.add(2.0);
  EXPECT_EQ(w.size(), 1u);
  EXPECT_DOUBLE_EQ(w.last(), 2.0);
}

// ---------------------------------------------------------------------------
// Counters

TEST(Counters, AddsGetsAndReadsUnknownAsZero) {
  Counters c;
  EXPECT_EQ(c.get("never"), 0);
  c.add("a");
  c.add("a", 4);
  c.add("b", -2);
  EXPECT_EQ(c.get("a"), 5);
  EXPECT_EQ(c.get("b"), -2);
  EXPECT_EQ(c.size(), 2u);
}

TEST(Counters, EachVisitsInNameOrderAndResetKeepsNames) {
  Counters c;
  c.add("zeta", 3);
  c.add("alpha", 1);
  std::vector<std::string> names;
  std::vector<int64_t> counts;
  c.each([&](std::string_view n, int64_t v) {
    names.emplace_back(n);
    counts.push_back(v);
  });
  EXPECT_EQ(names, (std::vector<std::string>{"alpha", "zeta"}));
  EXPECT_EQ(counts, (std::vector<int64_t>{1, 3}));
  c.reset();
  EXPECT_EQ(c.size(), 2u);
  EXPECT_EQ(c.get("zeta"), 0);
  c.clear();
  EXPECT_EQ(c.size(), 0u);
}

// ---------------------------------------------------------------------------
// Stopwatch, Laps and FrameTimer

// The one case in this file that reads the wall clock. It is here so
// that every other timing claim below is deterministic: sleeping and then
// asserting on a duration asserts the operating system's scheduler, which
// is not this library's to promise.
TEST(Timing, TheClocksAdvanceWithRealTimeAndResetSendsThemBack) {
  Stopwatch sw;
  Laps laps;
  double scoped = -1.0;
  {
    ScopedMs timed(scoped);
    std::this_thread::sleep_for(std::chrono::milliseconds(2));
  }
  const double slept = sw.elapsedMs();
  EXPECT_GE(slept, 1.0);
  EXPECT_GE(laps.mark("slept"), 1.0);
  EXPECT_GE(scoped, 1.0);
  // Reset is only observable once a span has been measured, which is why
  // it is claimed here rather than beside the deterministic cases. A
  // stopwatch that ignored reset would only ever read higher, so the
  // comparison is against the span already measured and not against a
  // ceiling the scheduler could cross.
  sw.reset();
  EXPECT_LT(sw.elapsedMs(), slept);
}

TEST(Laps, MarksAreNamedInOrderAndSumToTheTotal) {
  // Consecutive marks tile the span exactly — the end of one is the start
  // of the next — so their sum IS the total, whatever the durations turn
  // out to be.
  Laps laps;
  const double layout = laps.mark("layout");
  const double paint = laps.mark("paint");
  std::vector<std::string> names;
  double sum = 0.0;
  laps.each([&](std::string_view n, double ms) {
    names.emplace_back(n);
    sum += ms;
  });
  EXPECT_EQ(names, (std::vector<std::string>{"layout", "paint"}));
  EXPECT_EQ(laps.size(), 2u);
  EXPECT_DOUBLE_EQ(sum, layout + paint);
  EXPECT_DOUBLE_EQ(laps.totalMs(), sum);
}

// Whether `laps.mark(x)` compiles at all, for an argument of type T.
template <typename T, typename = void>
struct Marks : std::false_type {};
template <typename T>
struct Marks<
    T, std::void_t<decltype(std::declval<Laps&>().mark(std::declval<T>()))>>
    : std::true_type {};

TEST(Laps, RefuseANameThatWouldBeGoneBeforeItIsReadBack) {
  // The laps borrow their names, so a name built into a temporary would be
  // dangling by the time each() read it. That is a compile-time refusal,
  // not a value to assert on: what a caller may hand mark() is exactly a
  // name that outlives the timer.
  static_assert(Marks<const char*>::value, "a literal names a phase");
  static_assert(Marks<std::string_view>::value, "so does a view of one");
  static_assert(Marks<const std::string&>::value, "and a string it keeps");
  static_assert(!Marks<std::string>::value,
                "a string built into the call dies at the semicolon");
  static_assert(!Marks<std::string&&>::value,
                "and so does one that was moved from");

  // The borrowed name is read back, not a copy taken at the mark.
  std::string phase = "layout";
  Laps laps;
  laps.mark(phase);
  phase[0] = 'L';
  std::string seen;
  laps.each([&](std::string_view n, double) { seen = n; });
  EXPECT_EQ(seen, "Layout");
}

TEST(Laps, ResetForgetsTheMarksAndStartsThePhaseThere) {
  Laps laps;
  laps.mark("layout");
  laps.mark("paint");
  laps.reset();
  EXPECT_EQ(laps.size(), 0u);
  EXPECT_DOUBLE_EQ(laps.totalMs(), 0.0);
  laps.mark("again");
  EXPECT_EQ(laps.size(), 1u);
}

TEST(ScopedMs, LeavesItsTargetUntouchedUntilScopeExit) {
  // The target is assigned at destruction, not accumulated as the block
  // runs, so a caller may read the previous run's number until then.
  double ms = -1.0;
  {
    ScopedMs timed(ms);
    EXPECT_DOUBLE_EQ(ms, -1.0);
  }
  EXPECT_GE(ms, 0.0);
  EXPECT_NE(ms, -1.0);
}

TEST(FrameTimer, MarksFeedTheirLanes) {
  FrameTimer t(4);
  t.presented();  // seeds only
  EXPECT_TRUE(t.present().empty());
  t.begin();
  t.composed();
  t.finished();
  t.presented();
  EXPECT_EQ(t.work().size(), 1u);
  EXPECT_EQ(t.frame().size(), 1u);
  EXPECT_EQ(t.present().size(), 1u);
  EXPECT_GE(t.frame().last(), t.work().last());
  t.resetPresentation();
  EXPECT_TRUE(t.present().empty());
  EXPECT_EQ(t.work().size(), 1u);
  t.reset();
  EXPECT_TRUE(t.work().empty());
}

TEST(FrameTimer, HeadroomIsTheWorkCeiling) {
  FrameTimer t;
  EXPECT_DOUBLE_EQ(t.headroomFps(), 0.0);
  EXPECT_DOUBLE_EQ(t.presentedFps(), 0.0);
  t.addWork(4.0);
  t.addFrame(8.0);
  t.addPresent(20.0);
  EXPECT_DOUBLE_EQ(t.headroomFps(), 250.0);
  EXPECT_DOUBLE_EQ(t.presentedFps(), 50.0);
}

// ---------------------------------------------------------------------------
// Check

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

// ---------------------------------------------------------------------------
// The line a run of points is closest to

TEST(LineFit, APerfectLineIsFoundExactlyAndExplainsEverything) {
  const std::vector<double> xs{0, 1, 2, 3, 4};
  std::vector<double> ys;
  for (double x : xs) ys.push_back(3.5 + 2.25 * x);
  const LineFit<double> fit = lineFit<double>(xs, ys);
  EXPECT_NEAR(fit.slope, 2.25, 1e-12);
  EXPECT_NEAR(fit.intercept, 3.5, 1e-12);
  EXPECT_NEAR(fit.r2, 1.0, 1e-12);
  EXPECT_NEAR(fit.maxResidual, 0.0, 1e-12);
  EXPECT_NEAR(fit.rmsResidual, 0.0, 1e-12);
  EXPECT_EQ(fit.samples, 5u);
  EXPECT_NEAR(fit.at(10.0), 26.0, 1e-12);
}

TEST(LineFit, TheResidualIsWhatTurnsASlopeIntoEvidence) {
  // One point lifted off an otherwise exact line: the slope barely moves
  // and the residual is what says so.
  const std::vector<double> xs{0, 1, 2, 3, 4};
  std::vector<double> ys{0, 1, 2, 3, 4};
  ys[2] += 1.0;
  const LineFit<double> fit = lineFit<double>(xs, ys);
  EXPECT_NEAR(fit.slope, 1.0, 1e-12);
  EXPECT_GT(fit.maxResidual, 0.5);
  EXPECT_LT(fit.r2, 1.0);
  EXPECT_GT(fit.r2, 0.8) << "one lifted point still leaves a good line";
  EXPECT_NEAR(fit.residual(2.0, ys[2]), fit.maxResidual, 1e-12);
}

TEST(LineFit, WhatIsNotALineAnswersNoSlopeRatherThanADivideByZero) {
  const std::vector<double> flat{2, 2, 2, 2};
  const std::vector<double> ys{1, 5, -3, 9};
  const LineFit<double> vertical = lineFit<double>(flat, ys);
  EXPECT_EQ(vertical.slope, 0.0);
  EXPECT_NEAR(vertical.intercept, 3.0, 1e-12) << "the mean of the ordinates";
  EXPECT_EQ(vertical.r2, 0.0);
  EXPECT_NEAR(vertical.maxResidual, 6.0, 1e-12);
  // The residuals off that flat answer are the spread of the ordinates:
  // {1, 5, -3, 9} about 3 is {-2, 2, -6, 6}, so 80/4 under the root.
  EXPECT_NEAR(vertical.rmsResidual, std::sqrt(20.0), 1e-12);
  EXPECT_GT(vertical.rmsResidual, 0.0)
      << "a run with no slope still has ordinates that stand apart";

  const std::vector<double> one{7};
  const LineFit<double> single = lineFit<double>(one, one);
  EXPECT_EQ(single.slope, 0.0);
  EXPECT_NEAR(single.intercept, 7.0, 1e-12);
  EXPECT_DOUBLE_EQ(single.rmsResidual, 0.0)
      << "one point stands exactly on the answer through it";
  EXPECT_EQ(lineFit<double>({}, {}).samples, 0u);
}

TEST(LineFit, TheSumsAreAccumulatedInTheArgumentsOwnPrecision) {
  // A caller that has always fitted in float gets the float answer it
  // had, not a double one rounded back — which is the difference between
  // a drawing that holds and a drawing that moves by a sub-pixel.
  std::vector<float> xs, ys;
  for (int i = 0; i <= 80; ++i) {
    const float v = -27.0f + 70.0f * (float)i / 80.0f;
    xs.push_back(v);
    ys.push_back(std::log(std::tan((45.0f + v * 0.5f) * 0.017453293f)));
  }
  // The same accumulation the fit makes, written out: each point's
  // deviation from the means so far, folded in as it arrives, never the
  // sum of the squares less the square of the sum.
  float mx = 0, my = 0, sxx = 0, sxy = 0;
  for (size_t i = 0; i < xs.size(); ++i) {
    const float count = (float)(i + 1);
    const float dx = xs[i] - mx;
    const float dy = ys[i] - my;
    mx += dx / count;
    my += dy / count;
    sxx += dx * (xs[i] - mx);
    sxy += dx * (ys[i] - my);
  }
  const float b = sxy / sxx;
  const float a = my - b * mx;
  const LineFit<float> fit = lineFit<float>(xs, ys);
  EXPECT_EQ(fit.slope, b);
  EXPECT_EQ(fit.intercept, a);
}
