/** @file
 * The umbrella header: that every public header of the library still
 * compiles in one translation unit, and that the names each tier is
 * reached by are reachable through it alone.
 */

#include <gtest/gtest.h>

#include <filesystem>
#include <fstream>
#include <string>

#include "sigilgeometry/Geometry.h"

namespace {

using namespace sigil::geometry;

// A name from each tier, spelled through the umbrella and nothing else.
TEST(Umbrella, EveryTierIsReachableThroughTheOneInclude) {
  const path::Polyline square{.points = {{0, 0}, {10, 0}, {10, 10}, {0, 10}},
                              .closed = true};
  EXPECT_GT(square.length(), 0.0f);

  path::Cells<int> sheet(2, 2, 0);
  sheet.setBoundary(path::Boundary::Wrap);
  EXPECT_EQ(sheet.boundary(), path::Boundary::Wrap);

  const mesh::Mesh cube = mesh::box({0, 0, 0}, {1, 1, 1});
  EXPECT_FALSE(cube.positions.empty());

  const mesh::Cloud cloud;
  EXPECT_EQ(cloud.size(), 0u);
}

// A box-edge mask and a cell sheet's rule for a read outside are two
// different things in one namespace; the umbrella is the translation
// unit that has to hold both.
TEST(Umbrella, TheEdgeMaskAndTheCellBoundaryCoexist) {
  EXPECT_TRUE(path::has(path::Edge::All, path::Edge::Top));
  EXPECT_NE(path::Boundary::Clamp, path::Boundary::Constant);
}

}  // namespace

// The umbrella's own claim, read off the include tree: every header under
// the root is listed, bar the device ones the file's comment names, which
// hand in a GPU device and are taken deliberately. A header added to a
// tier without being listed here fails this case rather than being
// unreachable through the one include.
TEST(Umbrella, ListsEveryPublicHeader) {
  const std::filesystem::path root = SIGIL_GEOMETRY_INCLUDE_ROOT;
  std::ifstream umbrella(root / "sigilgeometry" / "Geometry.h");
  ASSERT_TRUE(umbrella.is_open());
  const std::string listed((std::istreambuf_iterator<char>(umbrella)),
                           std::istreambuf_iterator<char>());
  std::error_code ec;
  for (const auto& entry : std::filesystem::recursive_directory_iterator(
           root / "sigilgeometry", ec)) {
    if (!entry.is_regular_file() || entry.path().extension() != ".h") continue;
    const std::string spelled =
        entry.path().lexically_relative(root).generic_string();
    if (spelled == "sigilgeometry/Geometry.h") continue;
    if (spelled.find("/device/") != std::string::npos) continue;
    EXPECT_NE(listed.find("\"" + spelled + "\""), std::string::npos)
        << spelled << " is public and not in Geometry.h";
  }
}
