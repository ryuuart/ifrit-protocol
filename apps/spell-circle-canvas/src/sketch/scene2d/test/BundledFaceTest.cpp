/** @file
 * A font file a session's hub loads is a family the session's font
 * context knows: a run named in that family is set in the face the hub
 * holds, exactly as a run handed the face itself, and a file edited on
 * disk is the family's new face once the hub reads it again.
 */

#include <gtest/gtest.h>
#include <include/core/SkBitmap.h>
#include <include/core/SkSurface.h>
#include <sigilcompose/core/Factories.h>
#include <sigilio/advanced/Places.h>
#include <sigilio/hub/Hub.h>
#include <sigilmaterial/color/Color.h>
#include <sigilsketch/core/Assets.h>
#include <sigilsketch/scene2d/Scene2D.h>
#include <sigilweave/fonts/FontContext.h>
#include <sigilweave/ports/Resource.h>
#include <sigilweave/ports/SystemFontManager.h>
#include <sigilweave/style/Type.h>

#include <algorithm>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iterator>
#include <memory>
#include <span>
#include <string>
#include <utility>
#include <vector>

#include "ScratchDir.h"
#include "support/Pixels.h"

namespace {

using namespace sigil::sketch;
using namespace sigil::compose;
namespace fs = std::filesystem;
namespace weave = sigil::weave;

constexpr const char* kUri = "res://Face.ttf";

std::vector<char> fileBytes(const fs::path& path) {
  std::ifstream file(path, std::ios::binary);
  return {std::istreambuf_iterator<char>(file),
          std::istreambuf_iterator<char>()};
}

/** THE SAME FONT AT ANOTHER WEIGHT: the weight class its OS/2 table
 *  states rewritten, which keeps the family and changes the face. */
std::vector<char> atWeight(std::vector<char> bytes, uint16_t weight) {
  const auto read16 = [&bytes](size_t at) {
    return static_cast<uint16_t>((static_cast<uint8_t>(bytes[at]) << 8) |
                                 static_cast<uint8_t>(bytes[at + 1]));
  };
  const uint16_t tables = read16(4);
  for (uint16_t index = 0; index < tables; ++index) {
    const size_t record = 12 + size_t{index} * 16;
    if (std::string(&bytes[record], 4) != "OS/2") continue;
    const size_t offset =
        (size_t{read16(record + 8)} << 16) | read16(record + 10);
    bytes[offset + 4] = static_cast<char>(weight >> 8);
    bytes[offset + 5] = static_cast<char>(weight & 0xFF);
  }
  // The style's names, in both encodings the naming table holds, so the
  // new face is a face of its own to the platform and not the cached one.
  const auto rename = [&bytes](const std::string& from, const std::string& to) {
    for (size_t at = 0; at + from.size() <= bytes.size(); ++at)
      if (std::equal(from.begin(), from.end(), bytes.begin() + at))
        std::copy(to.begin(), to.end(), bytes.begin() + at);
  };
  const auto wide = [](const std::string& narrow) {
    std::string doubled;
    for (char character : narrow) doubled += {'\0', character};
    return doubled;
  };
  rename("Regular", "Boldest");
  rename(wide("Regular"), wide("Boldest"));
  return bytes;
}

void writeFile(const fs::path& path, const std::vector<char>& bytes) {
  std::ofstream file(path, std::ios::binary | std::ios::trunc);
  file.write(bytes.data(), static_cast<std::streamsize>(bytes.size()));
}

/** A body that loads the bundled face through its session's hub and
 *  sets one word as @p word describes it. */
struct Lettering final : Scene2DBody {
  std::function<Element(weave::Face)> word;
  void setup(Scene2DContext& ctx) override {
    ctx.canvas(220, 64);
    const auto face = ctx.assets.hub().load<weave::Face>(kUri);
    ctx.composer.render(
        box()
            .width(220)
            .height(64)
            .fill(Fill::color(sigil::material::Color{0, 0, 0, 1}))
            .children({word(face ? *face : weave::Face{})}));
  }
  void update(double, Scene2DContext&) override {}
};

/** The picture one frame of a session setting @p word leaves. */
SkBitmap plateOf(Assets& assets, weave::FontContext& fonts,
                 std::function<Element(weave::Face)> word) {
  auto body = std::make_unique<Lettering>();
  body->word = std::move(word);
  const std::unique_ptr<Session> session =
      openScene2D(std::move(body), fonts, assets, true, "lettering");
  const sk_sp<SkSurface> surface =
      SkSurfaces::Raster(SkImageInfo::MakeN32Premul(220, 64));
  session->frame(*surface->getCanvas(), 0.0);
  return sigil::sketch::test::plateOf(*surface);
}

Element byFamily(const std::string& family) {
  return text(u8"Waveform").fontSize(36).ink({1, 1, 1, 1}).fontFamily(family);
}

Element byFace(const weave::Face& face) {
  return text(u8"Waveform").fontSize(36).ink({1, 1, 1, 1}).font({.face = face});
}

TEST(BundledFace, AFamilyAHubLoadedIsSetInTheFaceTheHubHolds) {
  sigil::test::ScratchDir root("sketch_bundled_face");
  const fs::path file = root.path / "Face.ttf";
  writeFile(file, fileBytes(fs::path(SIGIL_TEST_INSTRUMENT_DIR) / "Sans.ttf"));
  Assets assets(root.path);
  weave::FontContext fonts(weave::ports::systemFontManager());
  const std::vector<char> written = fileBytes(file);
  const std::string family =
      weave::ports::faceFromBytes(std::as_bytes(std::span(written)))
          .familyName();
  ASSERT_FALSE(family.empty());
  // Named before any hub has read the file, the family is nobody's.
  ASSERT_FALSE(fonts.familyTypeface(family));

  const SkBitmap named = plateOf(
      assets, fonts, [&family](weave::Face) { return byFamily(family); });
  const auto held = assets.hub().load<weave::Face>(kUri);
  ASSERT_TRUE(held && *held);
  EXPECT_EQ(fonts.familyTypeface(family), *held);
  const SkBitmap faced =
      plateOf(assets, fonts, [](weave::Face face) { return byFace(face); });
  EXPECT_TRUE(sigil::sketch::test::samePicture(named, faced))
      << "the family set the word in another face than the one the hub holds";
  // `fontFamily(face)` is the face itself.
  const SkBitmap stated = plateOf(assets, fonts, [](weave::Face face) {
    return text(u8"Waveform").fontSize(36).ink({1, 1, 1, 1}).fontFamily(face);
  });
  EXPECT_TRUE(sigil::sketch::test::samePicture(stated, faced));
  const SkBitmap unnamed = plateOf(assets, fonts, [](weave::Face) {
    return text(u8"Waveform").fontSize(36).ink({1, 1, 1, 1});
  });
  EXPECT_FALSE(sigil::sketch::test::samePicture(named, unnamed))
      << "the bundled family drew as the default family does";
}

TEST(BundledFace, AnEditedFileIsTheFamilysNewFace) {
  sigil::test::ScratchDir root("sketch_bundled_face_edit");
  const fs::path file = root.path / "Face.ttf";
  const std::vector<char> regular =
      fileBytes(fs::path(SIGIL_TEST_INSTRUMENT_DIR) / "Sans.ttf");
  writeFile(file, regular);
  Assets assets(root.path);
  weave::FontContext fonts(weave::ports::systemFontManager());
  // A session stands while the file changes, as a live host's does.
  auto body = std::make_unique<Lettering>();
  body->word = [](weave::Face face) { return byFace(face); };
  const std::unique_ptr<Session> standing =
      openScene2D(std::move(body), fonts, assets, true, "lettering");
  const auto before = assets.hub().load<weave::Face>(kUri);
  ASSERT_TRUE(before && *before);
  const std::string family = before->familyName();
  ASSERT_EQ(fonts.familyTypeface(family), *before);

  writeFile(file, atWeight(regular, 700));
  fs::last_write_time(file,
                      fs::last_write_time(file) + std::chrono::seconds(5));
  ASSERT_TRUE(sigil::io::poll(assets.hub()));
  const auto after = assets.hub().load<weave::Face>(kUri);
  ASSERT_TRUE(after && *after);
  ASSERT_NE(*after, *before);
  EXPECT_EQ(fonts.familyTypeface(family), *after);
  // A session opened afterwards sets the family in the new face.
  const SkBitmap named = plateOf(
      assets, fonts, [&family](weave::Face) { return byFamily(family); });
  const SkBitmap faced =
      plateOf(assets, fonts, [](weave::Face face) { return byFace(face); });
  EXPECT_TRUE(sigil::sketch::test::samePicture(named, faced));
}

}  // namespace
