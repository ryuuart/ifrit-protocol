/** @file
 * The prefix search and the holder over it: which callables a signature
 * accepts, which it refuses, that the parameters a callable did not name
 * are still evaluated and dropped, and that every accepted spelling of one
 * drawing answers the same.
 */

#include <gtest/gtest.h>
#include <sigilcore/callable/Callable.h>

#include <string>

using sigil::core::Callable;
using sigil::core::callPrefix;
using sigil::core::namedParameters;
using sigil::core::PrefixCallable;

namespace {

struct Context {
  int weight = 1;
};

// ---------------------------------------------------------------------------
// The search itself, answered at compile time

using Paint = void(std::string&, const Context&);

static_assert(namedParameters<decltype([] {}), Paint> == 0);
static_assert(namedParameters<decltype([](std::string&) {}), Paint> == 1);
static_assert(
    namedParameters<decltype([](std::string&, const Context&) {}), Paint> == 2);
// A prefix runs from the FIRST parameter: the second alone is not one.
static_assert(namedParameters<decltype([](const Context&) {}), Paint> == -1);
static_assert(!PrefixCallable<decltype([](const Context&) {}), Paint>);
static_assert(PrefixCallable<decltype([](std::string&) {}), Paint>);
// A void result accepts any answer, so a callable that returns something
// is still a paint program.
static_assert(
    namedParameters<decltype([](std::string&) { return 7; }), Paint> == 1);
// A result that is NOT void has to be answered.
static_assert(namedParameters<decltype([] {}), int()> == -1);
static_assert(namedParameters<decltype([] { return 2; }), int()> == 0);
// The LONGEST prefix wins where a callable takes several.
static_assert(namedParameters<decltype([](auto&&...) {}), Paint> == 2);

}  // namespace

TEST(CoreCallable, CallsWithThePrefixTheCallableNamed) {
  std::string drawn;
  Context ctx{3};
  callPrefix([](std::string& out) { out += "one"; }, drawn, ctx);
  EXPECT_EQ(drawn, "one");
  callPrefix([](std::string& out,
                const Context& c) { out += std::to_string(c.weight); },
             drawn, ctx);
  EXPECT_EQ(drawn, "one3");
  callPrefix([&drawn] { drawn += "none"; }, drawn, ctx);
  EXPECT_EQ(drawn, "one3none");
}

TEST(CoreCallable, EveryAcceptedSpellingDrawsTheSame) {
  Callable<Paint> nullary = [] {};
  Callable<Paint> canvasOnly = [](std::string& out) { out += "x"; };
  Callable<Paint> both = [](std::string& out, const Context& c) {
    out += std::string((size_t)c.weight, 'x');
  };
  std::string a, b, c;
  Context ctx{1};
  nullary(a, ctx);
  canvasOnly(b, ctx);
  both(c, ctx);
  EXPECT_EQ(a, "");
  EXPECT_EQ(b, "x");
  EXPECT_EQ(c, "x");
}

TEST(CoreCallable, TheUnnamedParametersAreStillEvaluated) {
  // What a holder offers is built whether the callable reads it or not: the
  // offer is the verb's, and a caller that drops a parameter drops only the
  // reading of it.
  int built = 0;
  const auto offer = [&built] {
    ++built;
    return Context{2};
  };
  Callable<Paint> nullary = [] {};
  std::string out;
  nullary(out, offer());
  EXPECT_EQ(built, 1);
}

TEST(CoreCallable, AnEmptyHolderIsFalseAndAFilledOneIsTrue) {
  Callable<Paint> empty;
  EXPECT_FALSE((bool)empty);
  Callable<Paint> filled = [](std::string& out) { out += "!"; };
  EXPECT_TRUE((bool)filled);
  // A copy carries the call.
  Callable<Paint> copy = filled;
  std::string out;
  Context ctx;
  copy(out, ctx);
  EXPECT_EQ(out, "!");
}

TEST(CoreCallable, ACallableMayCarryItsOwnState) {
  // A holder calls what it holds as an lvalue, so a `mutable` callable — a
  // steppable with its own accumulator, a counter behind a part — is as good
  // a callable as a stateless one.
  Callable<int(int)> counted = [total = 0](int add) mutable {
    total += add;
    return total;
  };
  EXPECT_EQ(counted(2), 2);
  EXPECT_EQ(counted(3), 5);
  // And a copy carries the state it was copied at, not a shared one.
  Callable<int(int)> forked = counted;
  EXPECT_EQ(forked(1), 6);
  EXPECT_EQ(counted(1), 6);
}

TEST(CoreCallable, AResultIsCarriedBack) {
  Callable<int(int, int)> first = [](int a) { return a * 2; };
  EXPECT_EQ(first(21, 99), 42);
  Callable<int(int, int)> both = [](int a, int b) { return a + b; };
  EXPECT_EQ(both(1, 2), 3);
}
