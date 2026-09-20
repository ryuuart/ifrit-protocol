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

#include "../PipelineStore.h"

namespace fs = std::filesystem;

namespace {

/** A directory of this case's own, gone when the case is. */
class Scratch {
 public:
  explicit Scratch(const char* named)
      : m_path(fs::temp_directory_path() /
               ("sketchbook_pipeline_" + std::string(named))) {
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

TEST(SketchbookPipelineStore, AWrittenSetReadsBackKeyForKey) {
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

TEST(SketchbookPipelineStore, AMissingFileIsAnEmptySetAndNotAnError) {
  const Scratch scratch("missing");
  EXPECT_TRUE(
      pipelines::readKeySet(pipelines::keySetFile(scratch.path(), "nothing"))
          .empty());
}

TEST(SketchbookPipelineStore, AFileThatIsNotOneOfTheseIsRefusedWhole) {
  const Scratch scratch("stranger");
  const fs::path file = pipelines::keySetFile(scratch.path(), "stranger");
  std::FILE* open = std::fopen(file.c_str(), "wb");
  ASSERT_NE(open, nullptr);
  const char text[] = "this is a thumbnail, not a key set";
  std::fwrite(text, 1, sizeof text, open);
  std::fclose(open);
  EXPECT_TRUE(pipelines::readKeySet(file).empty());
}

TEST(SketchbookPipelineStore, ATruncatedFileAnswersWithTheKeysThatAreWhole) {
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

TEST(SketchbookPipelineStore, AKeyTheBackendCouldNotSerialiseIsNotWrittenDown) {
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

TEST(SketchbookPipelineStore, TheNameFollowsTheEffectsAndTheirOrder) {
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

TEST(SketchbookPipelineStore, TheNameIsTheSameForTheSameSourceRecompiled) {
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
