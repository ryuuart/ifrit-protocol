/** @file
 * The registry: what it records, the order it hands back, and how a name
 * on a command line resolves to an entry.
 */

#include <gtest/gtest.h>
#include <sigilsketch/core/Registry.h>
#include <sigilsketch/core/Session.h>

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

TEST(SketchRegistry, RecordsEveryEntry) {
  EXPECT_TRUE(kRegistered);
  EXPECT_EQ(registry().size(), 4u);
}

TEST(SketchRegistry, OrdersByCategoryThenName) {
  // Not registration order, which is link order and depends on nothing a
  // reader can see.
  ASSERT_EQ(registry().size(), 4u);
  EXPECT_STREQ(registry()[0].key, "alpha_wave");
  EXPECT_STREQ(registry()[1].key, "beta_decay");
  EXPECT_STREQ(registry()[2].key, "zebra_stripe");
  EXPECT_STREQ(registry()[3].key, "gated_thing");
}

TEST(SketchRegistry, FilesUnderItsStemUnlessToldOtherwise) {
  EXPECT_STREQ(registry()[1].name, "beta_decay");
  EXPECT_STREQ(registry()[0].name, "alpha wave");
}

TEST(SketchRegistry, FindsByIndexNameStemAndSubstring) {
  EXPECT_EQ(find("1"), 1);
  EXPECT_EQ(find("alpha wave"), 0);
  EXPECT_EQ(find("alpha_wave"), 0);  // the stem answers too
  EXPECT_EQ(find("ZEBRA"), 2);
  EXPECT_EQ(find("decay"), 1);
  EXPECT_EQ(find("nothing here"), -1);
  EXPECT_EQ(find(""), -1);
  EXPECT_EQ(find("99"), -1);
}

TEST(SketchRegistry, ExactMatchBeatsASubstringLaterInTheList) {
  // A name that is also a substring of another must not be captured by
  // whichever entry the sweep reaches first.
  EXPECT_EQ(find("beta_decay"), 1);
}

TEST(SketchRegistry, RefusesAnEntryWithNothingBehindIt) {
  EXPECT_FALSE(add(nullptr, nullptr, "c", "b", &stillKind));
  EXPECT_FALSE(add("key", nullptr, "c", "b", nullptr));
  EXPECT_EQ(registry().size(), 4u);
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
  EXPECT_TRUE(registry()[0].available(&why));
  EXPECT_EQ(why, "untouched");

  const Entry& gated = registry()[3];
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
  const Kind kind = registry()[0].kind();
  ASSERT_TRUE(kind);
  EXPECT_EQ(kind->runtime(), "still");
  // Two values built from one model compare equal, which is what lets a
  // kind be a comparable seam value rather than an identity.
  EXPECT_EQ(kind, registry()[1].kind());
}

}  // namespace
