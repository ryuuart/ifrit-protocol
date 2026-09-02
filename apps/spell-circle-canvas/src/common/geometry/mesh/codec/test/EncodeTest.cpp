/** @file
 * The PLY writer: clouds and meshes round-trip through the reader lane
 * for lane, binary rows survive exactly, header and rows agree when
 * lanes mismatch, and primitive lanes travel as face properties.
 */

#include <gtest/gtest.h>

#include <cmath>

#include "sigilgeometry/mesh/Mesh.h"
#include "sigilgeometry/mesh/codec/Decode.h"
#include "sigilgeometry/mesh/codec/Encode.h"
#include "sigilgeometry/mesh/pop/Pop.h"
#include "support/GeometrySupport.h"
#include <sigilgeometry/kit/Solids.h>

using namespace sigil::geometry;
using namespace sigil::geometry::mesh;

using sigil::geometry::test::splitQuad;

using codec::decode::Model;
using codec::decode::Part;

TEST(Save, PlyRoundTripsPrimitiveLanes) {
  // Per-triangle (prim) lanes survive a PLY round trip in both encodings:
  // they are written as face properties and read back into Mesh::prims
  // under the same names. Names are not special-cased — an arbitrary
  // "Charge" travels exactly like the conventional "Color".
  Mesh quad = mesh::quad(10, 6);  // 4 vertices, 2 triangles
  ASSERT_EQ(quad.triangleCount(), 2u);
  quad.prim("Color") = {{1, 0, 0, 1}, {0, 0.5f, 1, 0.25f}};
  quad.prim("Charge") = {{0.5f, -2, 7, 1.0f / 3.0f}, {1e-5f, 3, 0, 1}};

  for (const bool binary : {false, true}) {
    const std::string bytes = codec::encode::ply(quad, {.binary = binary});
    ASSERT_FALSE(bytes.empty());
    auto model = codec::decode::model(bytes.data(), bytes.size(), "prim.ply");
    ASSERT_TRUE(model.has_value());
    const codec::decode::Part& part = model->parts.front();
    const Mesh& back = part.mesh;
    ASSERT_EQ(back.triangleCount(), 2u);

    const std::vector<glm::vec4>* color = back.primIf("Color");
    ASSERT_NE(color, nullptr) << "binary=" << binary;
    ASSERT_EQ(color->size(), 2u);
    const std::vector<glm::vec4>* charge = back.primIf("Charge");
    ASSERT_NE(charge, nullptr) << "binary=" << binary;
    ASSERT_EQ(charge->size(), 2u);
    // The ascii writer prints with %g, which keeps six significant digits,
    // so ascii values come back close but not identical; binary rows are
    // raw floats and come back bit-exact.
    const float tol = binary ? 0.0f : 1e-6f;
    for (size_t t = 0; t < 2; ++t)
      for (int c = 0; c < 4; ++c) {
        EXPECT_NEAR((*color)[t][c], quad.prims.at("Color")[t][c], tol);
        EXPECT_NEAR((*charge)[t][c], quad.prims.at("Charge")[t][c],
                    std::abs(quad.prims.at("Charge")[t][c]) * tol + tol);
      }
    if (binary) {
      EXPECT_FLOAT_EQ((*charge)[0].w, 1.0f / 3.0f);
      EXPECT_FLOAT_EQ((*charge)[1].x, 1e-5f);
    }

    // Cardinality is preserved on the way back: a per-face lane has one
    // value per triangle, a point lane one per vertex, and the two never
    // mix. So the face lanes appear in Mesh::prims and nowhere else —
    // neither in the Part's point lanes nor in the Cloud it pours into.
    EXPECT_EQ(part.scalarLanes.count("Color_r"), 0u);
    EXPECT_EQ(part.colorLanes.count("Color"), 0u);
    EXPECT_EQ(part.colorLanes.count("Charge"), 0u);
    const Cloud cloud = part.asCloud();
    EXPECT_EQ(cloud.colorIf("Charge"), nullptr);

    // And merged() carries them out through Mesh::append.
    EXPECT_EQ(model->merged().prims.count("Charge"), 1u);
  }
}

TEST(Save, PlyRoundTripsCloudLanes) {
  // A Cloud carrying one of every lane kind writes to PLY and comes back
  // reconstituted: vectors fold from _x/_y/_z, colours from _r/_g/_b/_a,
  // and the conventional names keep their conventional spellings ("normal"
  // as nx/ny/nz, "tint" as red/green/blue/alpha). "tint" is the one lane
  // written as uchar channels, so it comes back quantized to 1/255 while
  // every other lane is float.
  Cloud cloud;
  cloud.positions = {{0, 0, 0}, {4, 0, 0}, {0, 4, 0}, {4, 4, 2}};
  cloud.scalar("energy") = {0.5f, 1.5f, 2.5f, 3.5f};
  cloud.vector("dir") = {{1, 0, 0}, {0, 1, 0}, {0, 0, 1}, {1, 0, 0}};
  cloud.vector("normal") = {{0, 0, 1}, {0, 0, 1}, {0, 0, 1}, {0, 0, 1}};
  cloud.color("glow") = {{0.25f, 0.5f, 0.75f, 1.0f},
                         {1, 0, 0, 0.5f},
                         {0, 1, 0, 0.25f},
                         {0, 0, 1, 0.125f}};
  cloud.color("tint") = {
      {1, 0, 0, 1}, {0, 1, 0, 1}, {0, 0, 1, 1}, {1, 1, 1, 0.5f}};

  const std::string bytes = codec::encode::ply(cloud);
  auto model = codec::decode::model(bytes.data(), bytes.size(), "trip.ply");
  ASSERT_TRUE(model.has_value());
  const Cloud back = model->parts.front().asCloud();
  ASSERT_EQ(back.size(), 4u);
  EXPECT_NEAR(back.positions[3].z, 2, 1e-4f);
  ASSERT_TRUE(back.scalarIf("energy"));
  EXPECT_FLOAT_EQ((*back.scalarIf("energy"))[2], 2.5f);
  ASSERT_TRUE(back.vectorIf("dir"));  // folded back from dir_x/_y/_z
  EXPECT_FLOAT_EQ((*back.vectorIf("dir"))[1].y, 1);
  ASSERT_TRUE(back.vectorIf("normal"));  // via nx/ny/nz
  EXPECT_FLOAT_EQ((*back.vectorIf("normal"))[0].z, 1);
  ASSERT_TRUE(back.colorIf("glow"));  // folded from glow_r/_g/_b/_a
  EXPECT_NEAR((*back.colorIf("glow"))[0].y, 0.5f, 1e-4f);
  EXPECT_NEAR((*back.colorIf("glow"))[3].w, 0.125f, 1e-4f);
  ASSERT_TRUE(back.colorIf("tint"));  // uchar red/green/blue/alpha
  EXPECT_NEAR((*back.colorIf("tint"))[1].y, 1, 1.5f / 255.0f);
  EXPECT_NEAR((*back.colorIf("tint"))[3].w, 0.5f, 1.5f / 255.0f);
}

TEST(Save, PlyRoundTripsMeshWithFaces) {
  Mesh quad = mesh::quad(10, 6);
  quad.colors.assign(quad.vertexCount(), {0.2f, 0.9f, 0.4f, 1});
  const std::string bytes = codec::encode::ply(quad);
  auto model = codec::decode::model(bytes.data(), bytes.size(), "quad.ply");
  ASSERT_TRUE(model.has_value());
  const Mesh& back = model->parts.front().mesh;
  ASSERT_EQ(back.vertexCount(), 4u);
  EXPECT_EQ(back.triangleCount(), 2u);
  glm::vec3 lo, hi;
  back.bounds(&lo, &hi);
  EXPECT_NEAR(hi.x - lo.x, 10, 1e-4f);
  EXPECT_NEAR(hi.y - lo.y, 6, 1e-4f);
  ASSERT_EQ(back.uvs.size(), 4u);
  ASSERT_EQ(back.colors.size(), 4u);
  EXPECT_NEAR(back.colors[0].y, 0.9f, 1.5f / 255.0f);
}

TEST(Save, BinaryPlyRoundTripsExactly) {
  // Binary rows are raw floats, so the round trip is BIT-exact and every
  // check can be an equality; only "tint", written as uchar channels, still
  // pays its quantization. The values are chosen to be ones the ascii
  // writer's six significant digits would have rounded — thirds, sevenths,
  // 1e-5 — so a silent fallback to the ascii path would fail here.
  Cloud cloud;
  cloud.positions = {{0.1f, 2.3f, -4.5f},
                     {6.7f, -8.9f, 10.11f},
                     {1.0f / 3.0f, 2.0f / 7.0f, 1e-5f}};
  cloud.scalar("energy") = {0.5f, 1.0f / 3.0f, 2.5f};
  cloud.vector("dir") = {{1, 0, 0}, {0.1f, 0.2f, 0.3f}, {0, 0, 1}};
  cloud.vector("normal") = {{0, 0, 1}, {0, 1, 0}, {1, 0, 0}};
  cloud.color("glow") = {
      {0.25f, 0.5f, 0.75f, 1.0f}, {1.0f / 3.0f, 0, 0, 0.5f}, {0, 1, 0, 0.125f}};
  cloud.color("tint") = {{1, 0, 0, 1}, {0, 1, 0, 1}, {1, 1, 1, 0.5f}};

  const std::string bytes = codec::encode::ply(cloud, {.binary = true});
  auto model = codec::decode::model(bytes.data(), bytes.size(), "bin.ply");
  ASSERT_TRUE(model.has_value());
  const Cloud back = model->parts.front().asCloud();
  ASSERT_EQ(back.size(), 3u);
  for (size_t i = 0; i < 3; ++i)
    for (int c = 0; c < 3; ++c)
      EXPECT_FLOAT_EQ(back.positions[i][c], cloud.positions[i][c]);
  ASSERT_TRUE(back.scalarIf("energy"));
  EXPECT_FLOAT_EQ((*back.scalarIf("energy"))[1], 1.0f / 3.0f);
  ASSERT_TRUE(back.vectorIf("dir"));  // folded back from dir_x/_y/_z
  EXPECT_FLOAT_EQ((*back.vectorIf("dir"))[1].z, 0.3f);
  ASSERT_TRUE(back.vectorIf("normal"));  // via nx/ny/nz
  EXPECT_FLOAT_EQ((*back.vectorIf("normal"))[1].y, 1.0f);
  ASSERT_TRUE(back.colorIf("glow"));  // folded from glow_r/_g/_b/_a
  EXPECT_FLOAT_EQ((*back.colorIf("glow"))[1].x, 1.0f / 3.0f);
  EXPECT_FLOAT_EQ((*back.colorIf("glow"))[2].w, 0.125f);
  ASSERT_TRUE(back.colorIf("tint"));  // uchar red/green/blue/alpha
  EXPECT_NEAR((*back.colorIf("tint"))[2].w, 0.5f, 1.5f / 255.0f);

  // Faces are written in the binary encoding too: a list count as one raw
  // uchar, then raw int32 indices — the same rows the ascii writer spells
  // out in text.
  Mesh quad = mesh::quad(10, 6);
  quad.colors.assign(quad.vertexCount(), {0.2f, 0.9f, 0.4f, 1});
  const std::string meshBytes = codec::encode::ply(quad, {.binary = true});
  auto meshModel =
      codec::decode::model(meshBytes.data(), meshBytes.size(), "quad.ply");
  ASSERT_TRUE(meshModel.has_value());
  const Mesh& tri = meshModel->parts.front().mesh;
  ASSERT_EQ(tri.vertexCount(), 4u);
  EXPECT_EQ(tri.triangleCount(), 2u);
  glm::vec3 lo, hi;
  tri.bounds(&lo, &hi);
  EXPECT_FLOAT_EQ(hi.x - lo.x, 10.0f);
  EXPECT_FLOAT_EQ(hi.y - lo.y, 6.0f);
  ASSERT_EQ(tri.colors.size(), 4u);
  EXPECT_NEAR(tri.colors[0].y, 0.9f, 1.5f / 255.0f);
}

TEST(Save, PlyHeaderAndRowsAgreeWhenLanesMismatchAndEmptyCloudDeclines) {
  // A lane whose length does not match the point count is skipped, and the
  // header and the row writer must agree on WHICH lanes those are — a
  // property declared in the header but not written on each row (or the
  // reverse) desyncs the file and makes every later value read wrong. The
  // export parsing at all is most of the assertion here.
  Cloud cloud;
  cloud.positions = {{0, 0, 0}, {1, 0, 0}, {0, 1, 0}};
  cloud.scalar("energy") = {1, 2, 3};
  cloud.scalars["stub"] = {7};                    // wrong length
  cloud.vectors["off"] = {{1, 2, 3}, {4, 5, 6}};  // wrong length
  const std::string bytes = codec::encode::ply(cloud);
  auto model = codec::decode::model(bytes.data(), bytes.size(), "skip.ply");
  ASSERT_TRUE(model.has_value());
  const Part& part = model->parts.front();
  ASSERT_EQ(part.mesh.vertexCount(), 3u);
  ASSERT_EQ(part.scalarLanes.count("energy"), 1u);
  EXPECT_FLOAT_EQ(part.scalarLanes.at("energy")[2], 3.0f);
  EXPECT_EQ(part.scalarLanes.count("stub"), 0u);
  EXPECT_EQ(part.vectorLanes.count("off"), 0u);
  EXPECT_EQ(part.scalarLanes.count("off_x"), 0u);

  // Nothing to write is refused rather than emitted: the string overloads
  // return empty and the file overloads return false, so no zero-element
  // PLY is ever produced — this library's own reader rejects one.
  EXPECT_TRUE(codec::encode::ply(Cloud{}).empty());
  EXPECT_TRUE(codec::encode::ply(Mesh{}).empty());
  const std::filesystem::path file = std::filesystem::temp_directory_path() /
                                     "sigilgeometry_empty_decline.ply";
  EXPECT_FALSE(codec::encode::ply(file, Cloud{}));
  EXPECT_FALSE(codec::encode::ply(file, Mesh{}));
}

TEST(Save, PlyWritesPrimLanesAsFaceProperties) {
  Mesh m = splitQuad();
  m.prim("Color")[0] = {1, 0, 0, 1};
  m.prim("Color")[1] = {0, 0.25f, 0, 1};
  const std::string text = codec::encode::ply(m);
  ASSERT_FALSE(text.empty());
  // Prim lanes are per-triangle, so they are declared on the FACE element,
  // and after the vertex_indices list because that is the order the rows
  // are written in. A reader walks properties in declaration order, so
  // header order and row order have to agree.
  const size_t face = text.find("element face 2");
  const size_t list = text.find("property list uchar int vertex_indices");
  const size_t prop = text.find("property float Color_r");
  ASSERT_NE(face, std::string::npos);
  ASSERT_NE(prop, std::string::npos);
  EXPECT_LT(face, list);
  EXPECT_LT(list, prop);
  EXPECT_NE(text.find("property float Color_a"), std::string::npos);
  // ...and written on each face row, after the three indices.
  EXPECT_NE(text.find("3 0 1 2 1 0 0 1\n"), std::string::npos);
  EXPECT_NE(text.find("3 0 2 3 0 0.25 0 1\n"), std::string::npos);
  // Adding face properties must not disturb the geometry: the written file
  // still reads back through this library's own importer with its triangles
  // intact. (That the prim VALUES also survive the trip is checked
  // separately, in PlyRoundTripsPrimitiveLanes.)
  auto back = codec::decode::model(text.data(), text.size(), "prims.ply");
  ASSERT_TRUE(back.has_value());
  ASSERT_EQ(back->parts.size(), 1u);
  EXPECT_EQ(back->parts.front().mesh.triangleCount(), 2u);
}
