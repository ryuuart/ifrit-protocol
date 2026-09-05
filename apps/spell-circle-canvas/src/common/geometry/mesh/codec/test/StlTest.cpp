/** @file
 * The STL reader: both encodings land the same triangle, and a binary
 * archive is told from an ascii one by its bytes rather than its name.
 */

#include <gtest/gtest.h>

#include <cstddef>
#include <cstring>
#include <string>
#include <vector>

#include "sigilgeometry/mesh/Mesh.h"
#include "sigilgeometry/mesh/codec/Decode.h"

using namespace sigil::geometry;
using namespace sigil::geometry::mesh;

using codec::decode::Model;
using codec::decode::Part;


namespace {

/** @p value's own bytes, appended — the way a binary container lays a
 *  scalar down. */
template <typename T>
void appendRaw(std::vector<std::byte>& out, const T& value) {
  const auto* begin = reinterpret_cast<const std::byte*>(&value);
  out.insert(out.end(), begin, begin + sizeof(T));
}

}  // namespace

TEST(ReadStl, BothEncodingsLandTheSameTriangle) {
  // STL stores a normal per facet, and zeroes are the file saying it has
  // none — the importer must then derive them from the winding rather than
  // publish a zero-length normal, which would shade black.
  std::vector<std::byte> stl(80, std::byte{0});
  appendRaw(stl, (uint32_t)2);
  const float tri[2][12] = {
      {0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 1, 0},
      {0, 0, 0, 0, 0, 0, 0, 1, 0, 1, 0, 0},
  };
  for (const float* f : {tri[0], tri[1]}) {
    for (int i = 0; i < 12; ++i) appendRaw(stl, f[i]);
    appendRaw(stl, (uint16_t)0);
  }
  auto model = codec::decode::model(stl.data(), stl.size(), "part.stl");
  ASSERT_TRUE(model.has_value());
  EXPECT_EQ(model->triangleCount(), 2u);
  // STL has no shared-vertex concept, so nothing is welded: every facet
  // keeps its own three vertices and the mesh stays flat shaded.
  EXPECT_EQ(model->vertexCount(), 6u);
  EXPECT_NEAR(glm::length(model->parts.front().mesh.normals.front()), 1, 1e-4);

  const std::string ascii = R"(solid tetra piece
facet normal 0 0 1
outer loop
vertex 0 0 0
vertex 1 0 0
vertex 0 1 0
endloop
endfacet
endsolid tetra piece
)";
  auto text = codec::decode::model(ascii.data(), ascii.size(), "part.stl");
  ASSERT_TRUE(text.has_value());
  EXPECT_EQ(text->triangleCount(), 1u);
  EXPECT_EQ(text->parts.front().name, "tetra piece");
  EXPECT_FLOAT_EQ(text->parts.front().mesh.normals.front().z, 1);
}
