/** @file
 * The OBJ reader: a model comes in as parts, its material library is
 * fetched through the resolver the caller supplied, and the base colour
 * it names reaches the part.
 */

#include <gtest/gtest.h>

#include <cstddef>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "sigilgeometry/mesh/Mesh.h"
#include "sigilgeometry/mesh/codec/Decode.h"
#include "support/GeometrySupport.h"

using namespace sigil::geometry;
using namespace sigil::geometry::mesh;

using codec::decode::Model;
using codec::decode::Part;

using sigil::geometry::test::kCubeMtl;
using sigil::geometry::test::toBytes;
using sigil::geometry::test::kCubeObj;

TEST(ReadObj, AMaterialLibraryIsFetchedThroughTheResolverAndReachesThePart) {
  std::vector<std::string> asked;
  const codec::decode::Resolver resolve =
      [&](std::string_view uri) -> std::optional<std::vector<std::byte>> {
    asked.emplace_back(uri);
    if (uri == "cube.mtl") return toBytes(kCubeMtl);
    return std::nullopt;
  };
  const std::string obj = kCubeObj;
  auto model =
      codec::decode::model(obj.data(), obj.size(), "cube.obj", resolve);
  ASSERT_TRUE(model.has_value());
  ASSERT_EQ(model->parts.size(), 1u);
  const Part& part = model->parts.front();
  EXPECT_EQ(part.name, "Cube");
  // Corners shared by several faces collapse to one vertex where their
  // (position, uv, normal) agree, so a cube stays 8 vertices instead of 24,
  // and OBJ's n-gon faces are fanned into triangles on the way in.
  EXPECT_EQ(part.mesh.vertexCount(), 8u);
  EXPECT_EQ(part.mesh.triangleCount(), 12u);
  EXPECT_FLOAT_EQ(part.baseColor.r, 1);
  EXPECT_FLOAT_EQ(part.baseColor.g, 0);
  // External references reach the caller through the resolver and nowhere
  // else — the importer never touches the filesystem itself. The uri is
  // passed through exactly as the file spelled it.
  ASSERT_EQ(asked.size(), 1u);
  EXPECT_EQ(asked.front(), "cube.mtl");
  // The file declares no normals, so they are computed; and the uv lane is
  // sized to the vertices even though the file has no vt records, because
  // consumers read lane size as the presence bit.
  ASSERT_EQ(part.mesh.normals.size(), 8u);
  EXPECT_NEAR(glm::length(part.mesh.normals.front()), 1, 1e-4);
  EXPECT_EQ(part.mesh.uvs.size(), 8u);
}
