/** @file
 * The recorded keys survive a launch, and a set recorded against a
 * different declaration is not mistaken for this one's.
 */

#include <gtest/gtest.h>
#include <include/core/SkData.h>
#include <include/core/SkString.h>
#include <include/effects/SkRuntimeEffect.h>

#include <cstdio>
#include <cstring>
#include <filesystem>
#include <string>
#include <system_error>
#include <vector>

#include "../src/PipelineStore.h"

namespace fs = std::filesystem;

namespace {

/** A directory of this case's own, gone when the case is. */
class Scratch {
 public:
  explicit Scratch(const char* named)
      : m_path(fs::temp_directory_path() /
               ("grimoire_pipeline_" + std::string(named))) {
    std::error_code ec;
    fs::remove_all(m_path, ec);
    fs::create_directories(m_path, ec);
  }
  ~Scratch() {
    std::error_code ec;
    fs::remove_all(m_path, ec);
  }
  Scratch(const Scratch&) = delete;
  Scratch& operator=(const Scratch&) = delete;

  const fs::path& path() const { return m_path; }

 private:
  fs::path m_path;
};

pipelines::RecordedPipeline keyOf(const char* text) {
  return {SkData::MakeWithCopy(text, std::strlen(text)),
          std::string("described as ") + text};
}

bool same(const SkData& left, const SkData& right) {
  return left.size() == right.size() &&
         std::memcmp(left.data(), right.data(), left.size()) == 0;
}

/** A body whose only purpose is to be a distinct runtime effect. */
sk_sp<SkRuntimeEffect> effectReturning(const char* colour) {
  const std::string body =
      std::string("half4 main(half4 c) { return ") + colour + "; }";
  auto [effect, error] =
      SkRuntimeEffect::MakeForColorFilter(SkString(body.c_str()));
  return effect;
}

}  // namespace

TEST(GrimoirePipelineStore, AWrittenSetReadsBackKeyForKey) {
  const Scratch scratch("roundtrip");
  const fs::path file = pipelines::keySetFile(scratch.path(), "abc123");
  const std::vector<pipelines::RecordedPipeline> written{
      keyOf("one"), keyOf("two-longer"), keyOf("three")};
  ASSERT_TRUE(pipelines::writeKeySet(file, written));
  const std::vector<pipelines::RecordedPipeline> read =
      pipelines::readKeySet(file);
  ASSERT_EQ(read.size(), written.size());
  for (size_t at = 0; at < read.size(); ++at) {
    EXPECT_TRUE(same(*read[at].key, *written[at].key));
    // The description travels with the key, because it is what says
    // the key still means what it meant.
    EXPECT_EQ(read[at].description, written[at].description);
  }
  // And nothing is left behind of the write itself: the set is written
  // beside and renamed over, so a reader never sees half of one.
  EXPECT_FALSE(fs::exists(fs::path(file).concat(".writing")));
}

TEST(GrimoirePipelineStore, RecordedEffectsReadBackInTheOrderWritten) {
  const Scratch scratch("effects");
  const fs::path file = pipelines::effectsFile(scratch.path());
  EXPECT_TRUE(pipelines::readEffects(file).empty());
  const std::vector<pipelines::RecordedEffect> written = {
      {pipelines::EffectKind::Shader,
       "half4 main(float2 xy) { return half4(1); }",
       {"crt_bloom"}},
      {pipelines::EffectKind::ColorFilter,
       "half4 main(half4 c) { return c.bgra; }",
       {}},
      {pipelines::EffectKind::Blender,
       "half4 main(half4 s, half4 d) { return s + d; }",
       {"ember_decode", "glass_forms"}}};
  ASSERT_TRUE(pipelines::writeEffects(file, written));
  EXPECT_EQ(pipelines::readEffects(file), written);
  // An effect is not a key set, so a store holding only effects holds no
  // set a launch could replay.
  EXPECT_FALSE(pipelines::holdsAnySketchSet(scratch.path(), ""));
}

TEST(GrimoirePipelineStore, AStoreFilledPastTheLimitNamesTheSketchesLeftUnkeyed) {
  // The backend declares a hundred effects, the stock ones first: a store
  // whose written-down effects run past the room left after them names
  // every sketch an effect beyond that room was written down for, and no
  // sketch whose effects all fit.
  const Scratch scratch("past_the_limit");
  const fs::path file = pipelines::effectsFile(scratch.path());
  constexpr size_t kLimit = 100;
  constexpr size_t kStock = 60;
  constexpr size_t kRoom = kLimit - kStock;
  std::vector<pipelines::RecordedEffect> written;
  for (size_t at = 0; at < kRoom + 30; ++at) {
    const std::string sketch = at < kRoom        ? "fits_whole"
                               : at < kRoom + 20 ? "past_the_room"
                                                 : "also_past";
    written.push_back({pipelines::EffectKind::ColorFilter,
                       "half4 main(half4 c) { return c * " +
                           std::to_string(at + 1) + ".0; }",
                       {sketch}});
  }
  // A sketch whose effects straddle the room is left unkeyed by the part
  // past it.
  written[kRoom - 1].sketches.push_back("straddles");
  written[kRoom].sketches.push_back("straddles");
  ASSERT_TRUE(pipelines::writeEffects(file, written));
  const std::vector<pipelines::RecordedEffect> read =
      pipelines::readEffects(file);
  ASSERT_EQ(read, written);
  EXPECT_EQ(pipelines::sketchesLeftUnkeyed(read, kRoom),
            (std::vector<std::string>{"also_past", "past_the_room",
                                      "straddles"}));
  // …and a store inside the room leaves none.
  EXPECT_TRUE(
      pipelines::sketchesLeftUnkeyed(std::span(read).first(kRoom), kRoom)
          .empty());
}

TEST(GrimoirePipelineStore, AMissingFileIsAnEmptySetAndNotAnError) {
  const Scratch scratch("missing");
  EXPECT_TRUE(
      pipelines::readKeySet(pipelines::keySetFile(scratch.path(), "nothing"))
          .empty());
}

TEST(GrimoirePipelineStore, AFileThatIsNotOneOfTheseIsRefusedWhole) {
  const Scratch scratch("stranger");
  const fs::path file = pipelines::keySetFile(scratch.path(), "stranger");
  std::FILE* open = std::fopen(file.c_str(), "wb");
  ASSERT_NE(open, nullptr);
  const char text[] = "this is a thumbnail, not a key set";
  std::fwrite(text, 1, sizeof text, open);
  std::fclose(open);
  EXPECT_TRUE(pipelines::readKeySet(file).empty());
}

TEST(GrimoirePipelineStore, ATruncatedFileAnswersWithTheKeysThatAreWhole) {
  const Scratch scratch("truncated");
  const fs::path file = pipelines::keySetFile(scratch.path(), "cut");
  ASSERT_TRUE(
      pipelines::writeKeySet(file, std::vector<pipelines::RecordedPipeline>{
                                       keyOf("first"), keyOf("second")}));
  const auto whole = fs::file_size(file);
  ASSERT_GT(whole, 8u);
  fs::resize_file(file, whole - 4);
  // Each key describes one program on its own, so a set that lost its
  // tail is a smaller set rather than a corrupt one.
  EXPECT_EQ(pipelines::readKeySet(file).size(), 1u);
}

TEST(GrimoirePipelineStore, AKeyTheBackendCouldNotSerialiseIsNotWrittenDown) {
  const Scratch scratch("nullkey");
  const fs::path file = pipelines::keySetFile(scratch.path(), "holes");
  ASSERT_TRUE(
      pipelines::writeKeySet(file, std::vector<pipelines::RecordedPipeline>{
                                       keyOf("kept"),
                                       {nullptr, "nothing to rebuild from"},
                                       {SkData::MakeEmpty(), "no bytes either"},
                                       keyOf("also")}));
  EXPECT_EQ(pipelines::readKeySet(file).size(), 2u);
}

TEST(GrimoirePipelineStore, TheNameFollowsTheEffectsAndTheirOrder) {
  const sk_sp<SkRuntimeEffect> red = effectReturning("half4(1, 0, 0, 1)");
  const sk_sp<SkRuntimeEffect> blue = effectReturning("half4(0, 0, 1, 1)");
  ASSERT_TRUE(red);
  ASSERT_TRUE(blue);
  const std::vector<sk_sp<SkRuntimeEffect>> forward{red, blue};
  const std::vector<sk_sp<SkRuntimeEffect>> backward{blue, red};
  const std::vector<sk_sp<SkRuntimeEffect>> shorter{red};
  const std::string name = pipelines::keySetName(forward, "metal");
  EXPECT_EQ(name, pipelines::keySetName(forward, "metal"));
  // A key names an effect by its place in the list, so the same bodies
  // in another order describe other programs.
  EXPECT_NE(name, pipelines::keySetName(backward, "metal"));
  EXPECT_NE(name, pipelines::keySetName(shorter, "metal"));
  EXPECT_NE(name, pipelines::keySetName(forward, "vulkan"));
}

TEST(GrimoirePipelineStore, TheNameIsTheSameForTheSameSourceRecompiled) {
  // The declaration is rebuilt every launch, so two runs hold different
  // effect OBJECTS for the same bodies and must still find their file.
  const sk_sp<SkRuntimeEffect> first = effectReturning("half4(0, 1, 0, 1)");
  const sk_sp<SkRuntimeEffect> second = effectReturning("half4(0, 1, 0, 1)");
  ASSERT_TRUE(first);
  ASSERT_TRUE(second);
  EXPECT_NE(first.get(), second.get());
  EXPECT_EQ(pipelines::keySetName({&first, 1}, "metal"),
            pipelines::keySetName({&second, 1}, "metal"));
}
