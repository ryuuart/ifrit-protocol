/** @file
 * The registry: what it records, the order it hands back, and how a name
 * on a command line resolves to an entry.
 */

#include <gtest/gtest.h>
#include <sigilsketch/core/Registry.h>
#include <sigilsketch/core/Session.h>

#include <string>
#include <string_view>
#include <vector>

namespace {

using namespace sigil::sketch;

/** A kind that draws nothing: the registry never opens one, so a test of
 *  what it holds needs no runtime at all. */
struct StillKind final : KindOps {
  [[nodiscard]] std::string_view runtime() const override { return "still"; }
  [[nodiscard]] std::unique_ptr<Session> open(sigil::weave::FontContext&,
                                              Assets&, bool) const override {
    return nullptr;
  }
  bool operator==(const StillKind&) const { return true; }
};

Kind stillKind() { return StillKind{}; }

/** A sketch over something the machine may not have. The probe is a
 *  static member, which is how a sketch states its own requirement. */
struct GatedSketch {
  static bool missing;
  static bool available(std::string* why) {
    if (missing && why) *why = "the thing it draws is not installed";
    return !missing;
  }
};
bool GatedSketch::missing = true;

/** …and one that needs nothing but the binary it is in. */
struct PlainSketch {};

/** Registered from a namespace-scope initializer, exactly as a sketch
 *  file's macro does. */
const bool kRegistered =
    add("zebra_stripe", nullptr, "Test \xc2\xb7 Later", "z", &stillKind) &&
    add("alpha_wave", "alpha wave", "Test \xc2\xb7 Early", "a", &stillKind) &&
    add("beta_decay", nullptr, "Test \xc2\xb7 Early", "b", &stillKind) &&
    add("gated_thing", nullptr, "Test \xc2\xb7 Zed", "g", &stillKind,
        &probeOf<GatedSketch>);

/** The registry is the process's, and every file registered into it is in
 *  there together, so these cases name their own entries by key rather
 *  than by position. */
const Entry& entry(std::string_view key) {
  for (const Entry& candidate : registry())
    if (candidate.key == key) return candidate;
  ADD_FAILURE() << "no entry registered under " << key;
  static const Entry kNone;
  return kNone;
}

/** Where an entry sits in the registry's own order. */
int index(std::string_view key) {
  int at = 0;
  for (const Entry& candidate : registry()) {
    if (candidate.key == key) return at;
    ++at;
  }
  return -1;
}

/** The four entries this file registered, in registry order. */
std::vector<std::string_view> ours() {
  std::vector<std::string_view> keys;
  for (const Entry& candidate : registry())
    if (std::string_view(candidate.category).starts_with("Test \xc2\xb7"))
      keys.emplace_back(candidate.key);
  return keys;
}

TEST(SketchRegistry, RecordsEveryEntry) {
  EXPECT_TRUE(kRegistered);
  EXPECT_EQ(ours().size(), 4u);
}

TEST(SketchRegistry, OrdersByCategoryThenName) {
  // Not registration order, which is link order and depends on nothing a
  // reader can see.
  EXPECT_EQ(ours(), (std::vector<std::string_view>{"alpha_wave", "beta_decay",
                                                   "zebra_stripe",
                                                   "gated_thing"}));
}

TEST(SketchRegistry, FilesUnderItsStemUnlessToldOtherwise) {
  EXPECT_STREQ(entry("beta_decay").name, "beta_decay");
  EXPECT_STREQ(entry("alpha_wave").name, "alpha wave");
}

TEST(SketchRegistry, FindsByIndexNameStemAndSubstring) {
  const int beta = index("beta_decay");
  EXPECT_EQ(find(std::to_string(beta)), beta);
  EXPECT_EQ(find("alpha wave"), index("alpha_wave"));
  EXPECT_EQ(find("alpha_wave"), index("alpha_wave"));  // the stem answers too
  EXPECT_EQ(find("ZEBRA"), index("zebra_stripe"));
  EXPECT_EQ(find("decay"), beta);
  EXPECT_EQ(find("nothing here"), -1);
  EXPECT_EQ(find(""), -1);
  EXPECT_EQ(find("9999"), -1);
}

TEST(SketchRegistry, ExactMatchBeatsASubstringLaterInTheList) {
  // A name that is also a substring of another must not be captured by
  // whichever entry the sweep reaches first.
  EXPECT_EQ(find("beta_decay"), index("beta_decay"));
}

TEST(SketchRegistry, RefusesAnEntryWithNothingBehindIt) {
  const size_t before = registry().size();
  EXPECT_FALSE(add(nullptr, nullptr, "c", "b", &stillKind));
  EXPECT_FALSE(add("key", nullptr, "c", "b", nullptr));
  EXPECT_EQ(registry().size(), before);
}

TEST(SketchAvailability, ReadsTheProbeOffTheSketchType) {
  // A type that declares one answers for itself; a type that declares
  // none is available wherever it compiled.
  std::string why;
  GatedSketch::missing = true;
  EXPECT_FALSE(probeOf<GatedSketch>(&why));
  EXPECT_FALSE(why.empty());
  GatedSketch::missing = false;
  EXPECT_TRUE(probeOf<GatedSketch>(nullptr));
  EXPECT_TRUE(probeOf<PlainSketch>(nullptr));
}

TEST(SketchAvailability, AnEntryAnswersForTheSketchBehindIt) {
  // An entry with no probe is available and leaves the reason alone; one
  // with a probe answers what the sketch says and names what is missing.
  std::string why = "untouched";
  EXPECT_TRUE(entry("alpha_wave").available(&why));
  EXPECT_EQ(why, "untouched");

  const Entry& gated = entry("gated_thing");
  GatedSketch::missing = true;
  why.clear();
  EXPECT_FALSE(gated.available(&why));
  EXPECT_EQ(why, "the thing it draws is not installed");
  GatedSketch::missing = false;
  EXPECT_TRUE(gated.available(nullptr));
}

TEST(SketchTitle, OpensUnderscoresIntoSpaces) {
  EXPECT_EQ(title("chaucer_astrolabe"), "chaucer astrolabe");
  EXPECT_EQ(title("aero desktop"), "aero desktop");
}

TEST(SketchKind, NamesTheRuntimeItDrawsThrough) {
  const Kind kind = entry("alpha_wave").kind();
  ASSERT_TRUE(kind);
  EXPECT_EQ(kind->runtime(), "still");
  // Two values built from one model compare equal, which is what lets a
  // kind be a comparable seam value rather than an identity.
  EXPECT_EQ(kind, entry("beta_decay").kind());
}

}  // namespace
