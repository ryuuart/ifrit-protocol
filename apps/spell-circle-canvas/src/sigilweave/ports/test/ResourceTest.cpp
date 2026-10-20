/** @file
 * A font file's bytes as a face: what a hub's face decoder reads.
 */

#include <gtest/gtest.h>
#include <include/core/SkFont.h>
#include <include/core/SkFontTypes.h>
#include <include/core/SkTypeface.h>
#include <sigilweave/ports/Resource.h>
#include <sigilweave/ports/SystemFontManager.h>

#include <cstddef>
#include <fstream>
#include <iterator>
#include <string>
#include <type_traits>
#include <vector>

#include "sigilweave/advanced/Skia.h"
#include "support/Faces.h"

namespace sigil::weave::ports {
namespace {

float advanceOf(const Face& face, char letter) {
  const SkFont font(toSk(face), 32);
  return font.measureText(&letter, 1, SkTextEncoding::kUTF8);
}

TEST(FaceResource, AFontFilesBytesAreTheFaceTheFileHolds) {
  const std::vector<std::byte> bytes = test::instrumentBytes("Sans.ttf");
  ASSERT_FALSE(bytes.empty());
  const Face read = faceFromBytes(bytes);
  ASSERT_TRUE(read);
  const std::string path = std::string(SIGIL_TEST_INSTRUMENT_DIR "/Sans.ttf");
  const Face opened = fromSk(systemFontManager()->makeFromFile(path.c_str()));
  ASSERT_TRUE(opened);
  EXPECT_EQ(read.familyName(), opened.familyName());
  EXPECT_FLOAT_EQ(advanceOf(read, 'W'), advanceOf(opened, 'W'));
}

TEST(FaceResource, BytesThatAreNoFontAreNoFace) {
  const std::vector<std::byte> noise(64, std::byte{0x2a});
  EXPECT_FALSE(faceFromBytes(noise));
  EXPECT_FALSE(faceFromBytes({}));
}

TEST(FaceResource, AFaceIsAskedForOnAHubUnderItsOwnName) {
  EXPECT_EQ(meaningName(std::type_identity<Face>{}), "weave.Face");
}

}  // namespace
}  // namespace sigil::weave::ports
