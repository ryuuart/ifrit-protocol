/** @file
 * The PLY reader: vertex properties fold into the lanes their names imply
 * in both encodings, face properties replicate across the triangles a fan
 * becomes, and a hostile header is refused rather than trusted.
 */

#include <gtest/gtest.h>

#include <cmath>
#include <cstring>
#include <string>
#include <string_view>
#include <vector>

#include "sigilgeometry/mesh/Mesh.h"
#include "sigilgeometry/mesh/codec/Decode.h"
#include "sigilgeometry/mesh/codec/Encode.h"

using namespace sigil::geometry;
using namespace sigil::geometry::mesh;

using codec::decode::Model;
using codec::decode::Part;


TEST(ReadPly, VertexPropertiesFoldIntoLanesInBothEncodings) {
  // PLY property routing: the conventional names (x/y/z, nx/ny/nz, s/t,
  // red/green/blue) build the mesh, and every other property becomes a lane
  // carrying its RAW value — no rescaling, so an integer id stays that
  // number. Only the colour properties are normalized, uchar 0..255 to
  // 0..1. A file with no face element imports as a point cloud with no
  // indices rather than being rejected.
  const char* ascii =
      "ply\n"
      "format ascii 1.0\n"
      "comment a houdini-ish scatter\n"
      "element vertex 4\n"
      "property float x\n"
      "property float y\n"
      "property float z\n"
      "property float intensity\n"
      "property uchar red\n"
      "property uchar green\n"
      "property uchar blue\n"
      "end_header\n"
      "0 0 0 0.5 255 0 0\n"
      "10 0 0 1.5 0 255 0\n"
      "0 10 0 2.5 0 0 255\n"
      "10 10 0 3.5 255 255 255\n";
  auto model = codec::decode::model(ascii, std::strlen(ascii), "scatter.ply");
  ASSERT_TRUE(model.has_value());
  const codec::decode::Part& part = model->parts.front();
  ASSERT_EQ(part.mesh.vertexCount(), 4u);
  EXPECT_TRUE(part.mesh.indices.empty());  // no faces declared, none invented
  EXPECT_FLOAT_EQ(part.mesh.positions[1].x, 10);
  ASSERT_EQ(part.mesh.colors.size(), 4u);
  EXPECT_NEAR(part.mesh.colors[0].r, 1, 1e-2f);  // uchar normalized
  EXPECT_NEAR(part.mesh.colors[2].b, 1, 1e-2f);
  const auto intensity = part.scalarLanes.find("intensity");
  ASSERT_NE(intensity, part.scalarLanes.end());
  EXPECT_FLOAT_EQ(intensity->second[3], 3.5f);

  // An imported lane is an ordinary lane: naming it as the scale lane makes
  // it drive instancing with no conversion step in between.
  const Cloud cloud = part.asCloud();
  points::InstanceOptions options;
  options.scaleLane = "intensity";
  const Mesh stamped = points::instance(cloud, mesh::quad(2, 2), options);
  EXPECT_EQ(stamped.vertexCount(), 4u * 4u);
  glm::vec3 lo, hi;
  stamped.bounds(&lo, &hi);
  // The last point's intensity is 3.5, so its 2x2 stamp reaches 3.5 beyond
  // the point at x = 10 — past anything the unscaled stamps could reach.
  EXPECT_GT(hi.x, 12.0f);

  // The binary_little_endian body encodes the same rows: properties in
  // declaration order, and a face list as a uchar count then int32 indices.
  std::vector<std::byte> bin;
  const auto push = [&](const void* p, size_t n) {
    const auto* b = static_cast<const std::byte*>(p);
    bin.insert(bin.end(), b, b + n);
  };
  const char* header =
      "ply\n"
      "format binary_little_endian 1.0\n"
      "element vertex 3\n"
      "property float x\n"
      "property float y\n"
      "property float z\n"
      "property float intensity\n"
      "element face 1\n"
      "property list uchar int vertex_indices\n"
      "end_header\n";
  push(header, std::strlen(header));
  const float verts[] = {0, 0, 0, 7, 4, 0, 0, 8, 0, 4, 0, 9};
  push(verts, sizeof(verts));
  const uint8_t faceCount = 3;
  const int32_t face[] = {0, 1, 2};
  push(&faceCount, 1);
  push(face, sizeof(face));
  auto binModel = codec::decode::model(bin.data(), bin.size(), "tri.ply");
  ASSERT_TRUE(binModel.has_value());
  const codec::decode::Part& tri = binModel->parts.front();
  EXPECT_EQ(tri.mesh.triangleCount(), 1u);
  EXPECT_FLOAT_EQ(tri.scalarLanes.at("intensity")[2], 9.0f);
  ASSERT_EQ(tri.mesh.normals.size(), 3u);
  EXPECT_NEAR(tri.mesh.normals.front().z, 1.0f, 1e-4f);
}

TEST(ReadPly, AHostileCountOrIndexIsRefusedRatherThanTrusted) {
  // (a) A face naming a vertex past the count is dropped whole — the
  // vertices still import, and computeNormals never indexes OOB.
  const char* badFace =
      "ply\nformat ascii 1.0\n"
      "element vertex 3\n"
      "property float x\nproperty float y\nproperty float z\n"
      "element face 1\n"
      "property list uchar int vertex_indices\n"
      "end_header\n"
      "0 0 0\n1 0 0\n0 1 0\n"
      "3 0 1 9\n";
  auto dropped = codec::decode::model(badFace, std::strlen(badFace), "bad.ply");
  ASSERT_TRUE(dropped.has_value());
  EXPECT_EQ(dropped->parts.front().mesh.vertexCount(), 3u);
  EXPECT_EQ(dropped->parts.front().mesh.triangleCount(), 0u);

  // (b) A vertex count no data could back is rejected before any
  // resize acts on it.
  const char* hugeCount =
      "ply\nformat ascii 1.0\n"
      "element vertex 4000000000\n"
      "property float x\nproperty float y\nproperty float z\n"
      "end_header\n"
      "0 0 0\n";
  EXPECT_FALSE(
      codec::decode::model(hugeCount, std::strlen(hugeCount), "huge.ply")
          .has_value());

  // (c) A binary list count promising more bytes than remain fails
  // the row read instead of walking off the buffer.
  std::vector<std::byte> truncated;
  const char* binHeader =
      "ply\nformat binary_little_endian 1.0\n"
      "element vertex 1\n"
      "property float x\nproperty float y\nproperty float z\n"
      "element face 1\n"
      "property list uchar int vertex_indices\n"
      "end_header\n";
  const auto pushBytes = [&](const void* p, size_t n) {
    const auto* b = static_cast<const std::byte*>(p);
    truncated.insert(truncated.end(), b, b + n);
  };
  pushBytes(binHeader, std::strlen(binHeader));
  const float vertex[3] = {0, 0, 0};
  pushBytes(vertex, sizeof(vertex));
  const uint8_t promised = 200;  // 800 bytes of indices; none follow
  pushBytes(&promised, 1);
  EXPECT_FALSE(
      codec::decode::model(truncated.data(), truncated.size(), "trunc.ply")
          .has_value());
}

TEST(ReadPly, APropertyNamedLikeOneAxisOfATripleStaysAScalarLane) {
  // In PLY, "t" is a texture coordinate only when it is paired with "s".
  // On its own it is an ordinary scalar property — and "t" is the name this
  // library's own point clouds use for the parameter along a curve, so
  // folding a lone "t" into uv.y would silently eat that lane on every file
  // this library writes.
  const char* ascii =
      "ply\nformat ascii 1.0\n"
      "element vertex 3\n"
      "property float x\nproperty float y\nproperty float z\n"
      "property float t\n"
      "end_header\n"
      "0 0 0 0.25\n1 0 0 0.5\n0 1 0 0.75\n";
  auto model = codec::decode::model(ascii, std::strlen(ascii), "lone_t.ply");
  ASSERT_TRUE(model.has_value());
  const Part& part = model->parts.front();
  const auto t = part.scalarLanes.find("t");
  ASSERT_NE(t, part.scalarLanes.end());
  ASSERT_EQ(t->second.size(), 3u);
  EXPECT_FLOAT_EQ(t->second[1], 0.5f);
  // The uv lane is still sized to the vertices, and still all zeroes: the
  // lone "t" went nowhere near it.
  for (const glm::vec2& uv : part.mesh.uvs) EXPECT_FLOAT_EQ(uv.y, 0.0f);
  const Cloud cloud = part.asCloud();
  ASSERT_TRUE(cloud.scalarIf("t"));
  EXPECT_FLOAT_EQ((*cloud.scalarIf("t"))[2], 0.75f);

  // And the loop closes: a cloud whose only extra lane is the scalar "t"
  // writes to PLY and reads back with its values intact.
  Cloud dump;
  dump.positions = {{0, 0, 0}, {2, 0, 0}, {0, 2, 0}};
  dump.scalar("t") = {0.1f, 0.6f, 0.9f};
  const std::string bytes = codec::encode::ply(dump);
  auto trip = codec::decode::model(bytes.data(), bytes.size(), "trip.ply");
  ASSERT_TRUE(trip.has_value());
  const Cloud back = trip->parts.front().asCloud();
  ASSERT_EQ(back.size(), 3u);
  ASSERT_TRUE(back.scalarIf("t"));
  EXPECT_FLOAT_EQ((*back.scalarIf("t"))[0], 0.1f);
  EXPECT_FLOAT_EQ((*back.scalarIf("t"))[1], 0.6f);
  EXPECT_FLOAT_EQ((*back.scalarIf("t"))[2], 0.9f);
}

TEST(ReadPly, APartialSuffixTripleStaysScalarAndRgbFoldsAlphaToOne) {
  // Properties named with an _x/_y/_z or _r/_g/_b/_a suffix are folded back
  // into a single vector or colour lane. The fold is all-or-nothing: an
  // incomplete triple stays as its raw scalar properties rather than
  // becoming a vector with an invented component, while an _r/_g/_b with no
  // _a folds with alpha 1, which is opaque.
  const char* ascii =
      "ply\nformat ascii 1.0\n"
      "element vertex 2\n"
      "property float x\nproperty float y\nproperty float z\n"
      "property float foo_x\nproperty float foo_y\n"
      "property float warm_r\nproperty float warm_g\n"
      "property float warm_b\n"
      "end_header\n"
      "0 0 0 1 2 0.25 0.5 0.75\n"
      "1 0 0 3 4 1 0 0.5\n";
  auto model = codec::decode::model(ascii, std::strlen(ascii), "fold.ply");
  ASSERT_TRUE(model.has_value());
  const Part& part = model->parts.front();
  EXPECT_EQ(part.vectorLanes.count("foo"), 0u);
  ASSERT_EQ(part.scalarLanes.count("foo_x"), 1u);
  ASSERT_EQ(part.scalarLanes.count("foo_y"), 1u);
  EXPECT_FLOAT_EQ(part.scalarLanes.at("foo_x")[1], 3.0f);
  EXPECT_FLOAT_EQ(part.scalarLanes.at("foo_y")[1], 4.0f);
  const auto warm = part.colorLanes.find("warm");
  ASSERT_NE(warm, part.colorLanes.end());
  ASSERT_EQ(warm->second.size(), 2u);
  EXPECT_FLOAT_EQ(warm->second[0].x, 0.25f);
  EXPECT_FLOAT_EQ(warm->second[0].y, 0.5f);
  EXPECT_FLOAT_EQ(warm->second[0].z, 0.75f);
  EXPECT_FLOAT_EQ(warm->second[0].w, 1.0f);
  EXPECT_EQ(part.scalarLanes.count("warm_r"), 0u);
  EXPECT_EQ(part.scalarLanes.count("warm_g"), 0u);
  EXPECT_EQ(part.scalarLanes.count("warm_b"), 0u);
}

TEST(ReadPly, AFacePropertyReplicatesAcrossTheTrianglesItsFanBecomes) {
  // The reader fan-triangulates an n-gon into n-2 triangles, so ONE source
  // face's per-face attribute has to be REPLICATED across all of them, or
  // the lane ends up sized to the face count instead of triangleCount() and
  // every value after the first n-gon is read against the wrong triangle. A
  // quad and a pentagon give 5 triangles from 2 face rows; a triangles-only
  // fixture could not tell the two sizings apart.
  const char* ascii =
      "ply\nformat ascii 1.0\n"
      "element vertex 6\n"
      "property float x\nproperty float y\nproperty float z\n"
      "element face 2\n"
      "property list uchar int vertex_indices\n"
      "property float Color_r\nproperty float Color_g\n"
      "property float Color_b\nproperty float Color_a\n"
      "property float density\n"
      "end_header\n"
      "0 0 0\n1 0 0\n1 1 0\n0 1 0\n2 1 0\n2 0 0\n"
      "4 0 1 2 3 1 0 0 1 2.5\n"
      "5 1 2 3 4 5 0 0.25 0.5 0.75 -1.5\n";
  auto model = codec::decode::model(ascii, std::strlen(ascii), "fan.ply");
  ASSERT_TRUE(model.has_value());
  const Mesh& mesh = model->parts.front().mesh;
  ASSERT_EQ(mesh.triangleCount(), 5u);  // 2 from the quad, 3 from the pent
  const std::vector<glm::vec4>* color = mesh.primIf("Color");
  ASSERT_NE(color, nullptr);
  ASSERT_EQ(color->size(), 5u);
  for (size_t t = 0; t < 2; ++t) {
    EXPECT_FLOAT_EQ((*color)[t].x, 1.0f);
    EXPECT_FLOAT_EQ((*color)[t].w, 1.0f);
  }
  for (size_t t = 2; t < 5; ++t) {
    EXPECT_FLOAT_EQ((*color)[t].x, 0.0f);
    EXPECT_FLOAT_EQ((*color)[t].y, 0.25f);
    EXPECT_FLOAT_EQ((*color)[t].z, 0.5f);
    EXPECT_FLOAT_EQ((*color)[t].w, 0.75f);
  }
  // Prim lanes are always four-component, so a single per-face scalar
  // property widens into .x with the other three left at zero.
  const std::vector<glm::vec4>* density = mesh.primIf("density");
  ASSERT_NE(density, nullptr);
  ASSERT_EQ(density->size(), 5u);
  EXPECT_FLOAT_EQ((*density)[1].x, 2.5f);
  EXPECT_FLOAT_EQ((*density)[2].x, -1.5f);
  EXPECT_FLOAT_EQ((*density)[4].x, -1.5f);
  EXPECT_FLOAT_EQ((*density)[0].y, 0.0f);

  // The binary encoding fans identically. A binary face row is a uchar
  // list count, then that many int32 indices, then the face's remaining
  // properties as raw floats.
  std::vector<std::byte> bin;
  const auto push = [&](const void* p, size_t n) {
    const auto* b = static_cast<const std::byte*>(p);
    bin.insert(bin.end(), b, b + n);
  };
  const char* header =
      "ply\nformat binary_little_endian 1.0\n"
      "element vertex 6\n"
      "property float x\nproperty float y\nproperty float z\n"
      "element face 2\n"
      "property list uchar int vertex_indices\n"
      "property float Color_r\nproperty float Color_g\n"
      "property float Color_b\nproperty float Color_a\n"
      "property float density\n"
      "end_header\n";
  push(header, std::strlen(header));
  const float verts[] = {0, 0, 0, 1, 0, 0, 1, 1, 0, 0, 1, 0, 2, 1, 0, 2, 0, 0};
  push(verts, sizeof(verts));
  const uint8_t quadCount = 4;
  const int32_t quadIdx[] = {0, 1, 2, 3};
  const float quadAttrs[] = {1, 0, 0, 1, 2.5f};
  push(&quadCount, 1);
  push(quadIdx, sizeof(quadIdx));
  push(quadAttrs, sizeof(quadAttrs));
  const uint8_t pentCount = 5;
  const int32_t pentIdx[] = {1, 2, 3, 4, 5};
  const float pentAttrs[] = {0, 0.25f, 0.5f, 0.75f, -1.5f};
  push(&pentCount, 1);
  push(pentIdx, sizeof(pentIdx));
  push(pentAttrs, sizeof(pentAttrs));
  auto binModel = codec::decode::model(bin.data(), bin.size(), "fan.ply");
  ASSERT_TRUE(binModel.has_value());
  const Mesh& binMesh = binModel->parts.front().mesh;
  ASSERT_EQ(binMesh.triangleCount(), 5u);
  ASSERT_NE(binMesh.primIf("Color"), nullptr);
  ASSERT_EQ(binMesh.primIf("Color")->size(), 5u);
  EXPECT_FLOAT_EQ((*binMesh.primIf("Color"))[1].x, 1.0f);
  EXPECT_FLOAT_EQ((*binMesh.primIf("Color"))[2].y, 0.25f);
  ASSERT_NE(binMesh.primIf("density"), nullptr);
  EXPECT_FLOAT_EQ((*binMesh.primIf("density"))[4].x, -1.5f);
}

TEST(ReadPly, FaceLanesTakeTheConventionalColourAndAnyDeclaredOrder) {
  // (a) Other tools spell per-face colour as red/green/blue/alpha rather
  // than Color_r/_g/_b/_a; both land in the same "Color" lane, with integer
  // channels normalized to 0..1 and a missing alpha filled with 1.
  // (b) Face properties may be declared BEFORE the index list. The whole
  // row is read before it is interpreted, so the triangle count a lane
  // replicates across does not depend on where the list sits.
  const char* ascii =
      "ply\nformat ascii 1.0\n"
      "element vertex 4\n"
      "property float x\nproperty float y\nproperty float z\n"
      "element face 2\n"
      "property uchar red\nproperty uchar green\n"
      "property uchar blue\n"
      "property float heat\n"
      "property list uchar int vertex_indices\n"
      "end_header\n"
      "0 0 0\n1 0 0\n1 1 0\n0 1 0\n"
      "255 0 0 9 3 0 1 2\n"
      "0 255 255 4 4 0 1 2 3\n";
  auto model = codec::decode::model(ascii, std::strlen(ascii), "meshlab.ply");
  ASSERT_TRUE(model.has_value());
  const Mesh& mesh = model->parts.front().mesh;
  ASSERT_EQ(mesh.triangleCount(), 3u);  // 1 triangle + a fanned quad
  const std::vector<glm::vec4>* color = mesh.primIf("Color");
  ASSERT_NE(color, nullptr);
  ASSERT_EQ(color->size(), 3u);
  EXPECT_NEAR((*color)[0].x, 1.0f, 1e-3f);  // uchar normalized
  EXPECT_NEAR((*color)[0].y, 0.0f, 1e-3f);
  EXPECT_FLOAT_EQ((*color)[0].w, 1.0f);  // no alpha channel -> 1
  EXPECT_NEAR((*color)[1].z, 1.0f, 1e-3f);
  EXPECT_NEAR((*color)[2].y, 1.0f, 1e-3f);
  const std::vector<glm::vec4>* heat = mesh.primIf("heat");
  ASSERT_NE(heat, nullptr);
  ASSERT_EQ(heat->size(), 3u);
  EXPECT_FLOAT_EQ((*heat)[0].x, 9.0f);  // raw: only colour names normalize
  EXPECT_FLOAT_EQ((*heat)[1].x, 4.0f);
  EXPECT_FLOAT_EQ((*heat)[2].x, 4.0f);
}

TEST(ReadPly, FaceLanesSurviveAHostileFaceHeader) {
  // (a) A face naming a vertex past the count is dropped whole, and
  // its per-face values go with it: the lane stays sized to
  // triangleCount() rather than carrying a phantom entry.
  const char* dropped =
      "ply\nformat ascii 1.0\n"
      "element vertex 3\n"
      "property float x\nproperty float y\nproperty float z\n"
      "element face 2\n"
      "property list uchar int vertex_indices\n"
      "property float Color_r\nproperty float Color_g\n"
      "property float Color_b\nproperty float Color_a\n"
      "end_header\n"
      "0 0 0\n1 0 0\n0 1 0\n"
      "3 0 1 9 1 1 1 1\n"
      "3 0 1 2 0.5 0.25 0.125 1\n";
  auto model = codec::decode::model(dropped, std::strlen(dropped), "drop.ply");
  ASSERT_TRUE(model.has_value());
  const Mesh& mesh = model->parts.front().mesh;
  ASSERT_EQ(mesh.triangleCount(), 1u);
  ASSERT_NE(mesh.primIf("Color"), nullptr);
  ASSERT_EQ(mesh.primIf("Color")->size(), 1u);
  // The SURVIVING face's value, not the dropped one's.
  EXPECT_FLOAT_EQ((*mesh.primIf("Color"))[0].x, 0.5f);

  // (b) A face count no data could back is rejected before anything is
  // sized from it — the prim path never resizes on a declared count.
  const char* hugeFaces =
      "ply\nformat ascii 1.0\n"
      "element vertex 3\n"
      "property float x\nproperty float y\nproperty float z\n"
      "element face 4000000000\n"
      "property list uchar int vertex_indices\n"
      "property float Color_r\nproperty float Color_g\n"
      "property float Color_b\nproperty float Color_a\n"
      "end_header\n"
      "0 0 0\n1 0 0\n0 1 0\n"
      "3 0 1 2 1 1 1 1\n";
  EXPECT_FALSE(
      codec::decode::model(hugeFaces, std::strlen(hugeFaces), "huge.ply")
          .has_value());

  // (c) A header promising more face rows than the body delivers fails
  // the read instead of publishing a short lane.
  const char* shortBody =
      "ply\nformat ascii 1.0\n"
      "element vertex 3\n"
      "property float x\nproperty float y\nproperty float z\n"
      "element face 3\n"
      "property list uchar int vertex_indices\n"
      "property float Color_r\nproperty float Color_g\n"
      "property float Color_b\nproperty float Color_a\n"
      "end_header\n"
      "0 0 0\n1 0 0\n0 1 0\n"
      "3 0 1 2 1 1 1 1\n";
  EXPECT_FALSE(
      codec::decode::model(shortBody, std::strlen(shortBody), "short.ply")
          .has_value());

  // (d) A duplicate face property claims its lane once: the second
  // declaration is read and discarded rather than appending a second
  // time and desyncing the lane off triangleCount().
  const char* duplicate =
      "ply\nformat ascii 1.0\n"
      "element vertex 3\n"
      "property float x\nproperty float y\nproperty float z\n"
      "element face 1\n"
      "property list uchar int vertex_indices\n"
      "property float heat\nproperty float heat\n"
      "end_header\n"
      "0 0 0\n1 0 0\n0 1 0\n"
      "3 0 1 2 6 7\n";
  auto dupModel =
      codec::decode::model(duplicate, std::strlen(duplicate), "dup.ply");
  ASSERT_TRUE(dupModel.has_value());
  const Mesh& dupMesh = dupModel->parts.front().mesh;
  ASSERT_EQ(dupMesh.triangleCount(), 1u);
  ASSERT_NE(dupMesh.primIf("heat"), nullptr);
  ASSERT_EQ(dupMesh.primIf("heat")->size(), 1u);
  EXPECT_FLOAT_EQ((*dupMesh.primIf("heat"))[0].x, 6.0f);
}
