/** @file
 * The writers: a cloud and a mesh come back through the reader lane for
 * lane in either PLY encoding and in `.geo`, binary rows survive bit for
 * bit, header and rows agree when lanes mismatch, primitive lanes travel
 * as face properties, and what this library writes is recognisable from
 * its bytes alone.
 */

#include <gtest/gtest.h>

#include <cmath>
#include <filesystem>
#include <string>

#include "sigilgeometry/mesh/Mesh.h"
#include "sigilgeometry/mesh/codec/Decode.h"
#include "sigilgeometry/mesh/codec/Encode.h"
#include "sigilgeometry/mesh/pop/Pop.h"
#include "ScratchDir.h"
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

// The mesh round trip in both encodings: the writer emits a face element
// either way — a list count then the indices, spelled out in text or laid
// down as raw bytes — and the reader brings back the same quad.
class PlyEncoding : public ::testing::TestWithParam<bool> {};

TEST_P(PlyEncoding, RoundTripsAMeshWithItsFacesAndColours) {
  Mesh quad = mesh::quad(10, 6);
  quad.colors.assign(quad.vertexCount(), {0.2f, 0.9f, 0.4f, 1});
  const std::string bytes = codec::encode::ply(quad, {.binary = GetParam()});
  ASSERT_FALSE(bytes.empty());
  auto model = codec::decode::model(bytes.data(), bytes.size(), "quad.ply");
  ASSERT_TRUE(model.has_value());
  const Mesh& back = model->parts.front().mesh;
  ASSERT_EQ(back.vertexCount(), 4u);
  EXPECT_EQ(back.triangleCount(), 2u);
  glm::vec3 lo, hi;
  back.bounds(&lo, &hi);
  EXPECT_FLOAT_EQ(hi.x - lo.x, 10.0f);
  EXPECT_FLOAT_EQ(hi.y - lo.y, 6.0f);
  ASSERT_EQ(back.uvs.size(), 4u);
  ASSERT_EQ(back.colors.size(), 4u);
  // "tint" travels as uchar channels either way, so it pays its
  // quantization in both.
  EXPECT_NEAR(back.colors[0].y, 0.9f, 1.5f / 255.0f);
}

INSTANTIATE_TEST_SUITE_P(
    Ply, PlyEncoding, ::testing::Bool(),
    [](const ::testing::TestParamInfo<bool>& info) {
      return info.param ? "Binary" : "Ascii";
    });

TEST(Save, ABinaryPlyRoundTripsACloudBitForBit) {
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
  const sigil::test::ScratchDir scratch("geometry_encode");
  const std::filesystem::path file = scratch.path / "decline.ply";
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

// A blob arriving over the wire, out of a cache, or from a URL whose path
// ends in nothing carries no extension for `model()` to dispatch on, and
// then the bytes have to speak for themselves. Every format this library
// WRITES must be first among those: a file it made has to come back
// through a hint that says nothing about it.
struct WrittenFormat {
  const char* name;
  std::string (*write)(const Mesh&);
};

class WrittenBytes : public ::testing::TestWithParam<WrittenFormat> {};

TEST_P(WrittenBytes, AreSniffableWithNoExtensionToGoOn) {
  const Mesh mesh = splitQuad();
  const std::string bytes = GetParam().write(mesh);
  ASSERT_FALSE(bytes.empty());
  for (const char* hint : {"", "download", "dir.d/blob", "blob.dat"}) {
    const auto back = codec::decode::model(bytes.data(), bytes.size(), hint);
    ASSERT_TRUE(back.has_value()) << "under hint '" << hint << "'";
    ASSERT_EQ(back->parts.size(), 1u);
    // The triangles are what every format carries back; whether the
    // vertices come back welded is each writer's own claim.
    EXPECT_EQ(back->parts.front().mesh.triangleCount(), mesh.triangleCount());
  }
}

INSTANTIATE_TEST_SUITE_P(
    Save, WrittenBytes,
    ::testing::Values(
        WrittenFormat{"PlyAscii",
                      [](const Mesh& m) { return codec::encode::ply(m); }},
        WrittenFormat{"PlyBinary",
                      [](const Mesh& m) {
                        return codec::encode::ply(m, {.binary = true});
                      }},
        WrittenFormat{"Geo",
                      [](const Mesh& m) { return codec::encode::geo(m); }}),
    [](const ::testing::TestParamInfo<WrittenFormat>& info) {
      return std::string(info.param.name);
    });

TEST(Save, GeoRoundTripsACloudLaneForLane) {
  // The writer is the reader's return leg, so the assertion is that a
  // cloud comes back as itself: the conventional lanes under the names
  // that side knows them by, and every other lane at the width that
  // brings it back as the same KIND of lane.
  mesh::Cloud cloud;
  cloud.positions = {{0, 0, 0}, {1, 2, 3}, {-4.5f, 0.25f, 7}};
  cloud.vector("normal") = {{0, 1, 0}, {1, 0, 0}, {0, 0, 1}};
  cloud.color("tint") = {{1, 0, 0, 1}, {0, 1, 0, 0.5f}, {0, 0, 1, 0.25f}};
  cloud.color("uv") = {{0, 0, 0, 0}, {0.5f, 0.25f, 0, 0}, {1, 1, 0, 0}};
  cloud.scalar("ring") = {0, 1, 1};
  cloud.vector("velocity") = {{1, 1, 1}, {2, 0, 0}, {0, -3, 0}};

  const std::string text = codec::encode::geo(cloud);
  ASSERT_FALSE(text.empty());
  const auto back = codec::decode::model(text.data(), text.size(), "out.geo");
  ASSERT_TRUE(back.has_value());
  ASSERT_EQ(back->parts.size(), 1u);
  const mesh::Cloud again = back->parts.front().asCloud();

  ASSERT_EQ(again.size(), cloud.size());
  for (size_t i = 0; i < cloud.size(); ++i) {
    EXPECT_NEAR(again.positions[i].x, cloud.positions[i].x, 1e-5f);
    EXPECT_NEAR(again.positions[i].z, cloud.positions[i].z, 1e-5f);
  }
  const std::vector<glm::vec3>* normal = again.vectorIf("normal");
  ASSERT_NE(normal, nullptr);
  EXPECT_NEAR((*normal)[0].y, 1.0f, 1e-5f);
  const std::vector<glm::vec4>* tint = again.colorIf("tint");
  ASSERT_NE(tint, nullptr);
  EXPECT_NEAR((*tint)[1].y, 1.0f, 1e-5f);
  EXPECT_NEAR((*tint)[2].w, 0.25f, 1e-5f) << "the alpha rides in Cd";
  const std::vector<glm::vec4>* uv = again.colorIf("uv");
  ASSERT_NE(uv, nullptr);
  EXPECT_NEAR((*uv)[1].x, 0.5f, 1e-5f);
  EXPECT_NEAR((*uv)[1].y, 0.25f, 1e-5f) << "the v flip is undone on the way out";
  // A lane that arrived as a group leaves as the scalar it became, which
  // is what a mask reads either way.
  const std::vector<float>* ring = again.scalarIf("ring");
  ASSERT_NE(ring, nullptr);
  EXPECT_EQ(ring->size(), 3u);
  EXPECT_NEAR((*ring)[1], 1.0f, 1e-5f);
  const std::vector<glm::vec3>* velocity = again.vectorIf("velocity");
  ASSERT_NE(velocity, nullptr);
  EXPECT_NEAR((*velocity)[2].y, -3.0f, 1e-5f);

  // An empty cloud declines, on the same terms the PLY writer declines.
  EXPECT_TRUE(codec::encode::geo(mesh::Cloud{}).empty());
}

TEST(Save, GeoRoundTripsAMeshUnweldedWithItsPrimitiveLanes) {
  Mesh mesh = splitQuad();
  mesh.prim("Color") = {{1, 0, 0, 1}, {0, 0.25f, 0, 1}};
  mesh.prim("Charge") = {{7, 0, 0, 0}, {-2, 0, 0, 0}};

  const std::string text = codec::encode::geo(mesh);
  ASSERT_FALSE(text.empty());
  const auto back = codec::decode::model(text.data(), text.size(), "out.geo");
  ASSERT_TRUE(back.has_value());
  ASSERT_EQ(back->parts.size(), 1u);
  const Mesh& again = back->parts.front().mesh;

  // The faces and the shape survive; the vertex COUNT does not, and that
  // is the format: a .geo addresses a polygon's corners through a vertex
  // list, and every corner gets its own mesh vertex so a per-corner uv or
  // normal can survive a seam.
  EXPECT_EQ(again.triangleCount(), mesh.triangleCount());
  EXPECT_EQ(again.vertexCount(), mesh.triangleCount() * 3);
  for (size_t t = 0; t < again.triangleCount(); ++t)
    for (int c = 0; c < 3; ++c) {
      const glm::vec3 was = mesh.positions[mesh.indices[t * 3 + (size_t)c]];
      const glm::vec3 is = again.positions[again.indices[t * 3 + (size_t)c]];
      EXPECT_NEAR(was.x, is.x, 1e-5f);
      EXPECT_NEAR(was.y, is.y, 1e-5f);
      EXPECT_NEAR(was.z, is.z, 1e-5f);
    }

  const std::vector<glm::vec4>* colour = again.primIf("Color");
  ASSERT_NE(colour, nullptr);
  EXPECT_NEAR((*colour)[1].y, 0.25f, 1e-5f);
  // Four components under its own name, so a lane the reader has no
  // convention for still comes back whole rather than splatted.
  const std::vector<glm::vec4>* charge = again.primIf("Charge");
  ASSERT_NE(charge, nullptr);
  EXPECT_NEAR((*charge)[0].x, 7.0f, 1e-5f);
  EXPECT_NEAR((*charge)[1].x, -2.0f, 1e-5f);

  // A mesh with no faces IS a point cloud, and there is one spelling of
  // that here rather than two.
  Mesh bare;
  bare.positions = mesh.positions;
  const std::string cloudText = codec::encode::geo(bare);
  const auto asCloud =
      codec::decode::model(cloudText.data(), cloudText.size(), "bare.geo");
  ASSERT_TRUE(asCloud.has_value());
  EXPECT_EQ(asCloud->parts.front().mesh.vertexCount(), bare.vertexCount());
  EXPECT_EQ(asCloud->parts.front().mesh.triangleCount(), 0u);
}
