#include <Alembic/Abc/All.h>
#include <Alembic/AbcCoreOgawa/All.h>
#include <Alembic/AbcGeom/All.h>
#include <gtest/gtest.h>

#include <cmath>
#include <glm/gtc/matrix_transform.hpp>

#include "sigilgeometry/Import.h"
#include "sigilgeometry/Mesh.h"
#include "sigilgeometry/Pop.h"
#include "sigilgeometry/Save.h"
#include "support/GeometrySupport.h"

using namespace sigil::geometry;

using sigil::geometry::test::kCubeMtl;
using sigil::geometry::test::kCubeObj;
using sigil::geometry::test::splitQuad;

namespace {

using import::Model;
using import::Part;

std::vector<std::byte> toBytes(std::string_view text) {
  const auto* begin = reinterpret_cast<const std::byte*>(text.data());
  return {begin, begin + text.size()};
}

std::string base64(const std::vector<std::byte>& bytes) {
  static const char* alphabet =
      "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
  std::string out;
  for (size_t i = 0; i < bytes.size(); i += 3) {
    uint32_t chunk = (uint32_t)bytes[i] << 16;
    if (i + 1 < bytes.size()) chunk |= (uint32_t)bytes[i + 1] << 8;
    if (i + 2 < bytes.size()) chunk |= (uint32_t)bytes[i + 2];
    out.push_back(alphabet[(chunk >> 18) & 63]);
    out.push_back(alphabet[(chunk >> 12) & 63]);
    out.push_back(i + 1 < bytes.size() ? alphabet[(chunk >> 6) & 63] : '=');
    out.push_back(i + 2 < bytes.size() ? alphabet[chunk & 63] : '=');
  }
  return out;
}

template <typename T>
void appendRaw(std::vector<std::byte>& out, const T& value) {
  const auto* begin = reinterpret_cast<const std::byte*>(&value);
  out.insert(out.end(), begin, begin + sizeof(T));
}

/** One triangle at (0,0,0) (1,0,0) (0,1,0), uint16 indices 0 1 2. */
std::vector<std::byte> triangleBufferBytes() {
  std::vector<std::byte> bin;
  const float positions[9] = {0, 0, 0, 1, 0, 0, 0, 1, 0};
  for (float f : positions) appendRaw(bin, f);
  for (uint16_t i : {uint16_t(0), uint16_t(1), uint16_t(2)}) appendRaw(bin, i);
  return bin;
}

/** The minimal glTF scene: one node, translated +10 along x, holding one red
 *  triangle. The node transform is deliberately non-identity so tests can
 *  tell whether it was applied. @p bufferUri empty writes no uri member,
 *  which is how a GLB's embedded buffer is spelled. */
std::string triangleGltfJson(const std::string& bufferUri) {
  std::string buffer = "{\"byteLength\": 42";
  if (!bufferUri.empty()) buffer += ", \"uri\": \"" + bufferUri + "\"";
  buffer += "}";
  return R"({
  "asset": {"version": "2.0"},
  "scene": 0,
  "scenes": [{"nodes": [0]}],
  "nodes": [{"mesh": 0, "name": "tri", "translation": [10, 0, 0]}],
  "meshes": [{"primitives": [
    {"attributes": {"POSITION": 0}, "indices": 1, "material": 0}]}],
  "materials": [{"pbrMetallicRoughness":
    {"baseColorFactor": [1, 0, 0, 1]}}],
  "buffers": [)" +
         buffer + R"(],
  "bufferViews": [
    {"buffer": 0, "byteOffset": 0, "byteLength": 36},
    {"buffer": 0, "byteOffset": 36, "byteLength": 6}],
  "accessors": [
    {"bufferView": 0, "componentType": 5126, "count": 3, "type": "VEC3",
     "min": [0, 0, 0], "max": [1, 1, 0]},
    {"bufferView": 1, "componentType": 5123, "count": 3,
     "type": "SCALAR"}]
})";
}

std::vector<std::byte> glbBytes() {
  std::string json = triangleGltfJson("");
  while (json.size() % 4) json.push_back(' ');
  std::vector<std::byte> bin = triangleBufferBytes();
  while (bin.size() % 4) bin.push_back(std::byte{0});
  std::vector<std::byte> out;
  appendRaw(out, (uint32_t)0x46546C67);  // "glTF"
  appendRaw(out, (uint32_t)2);
  appendRaw(out, (uint32_t)(12 + 8 + json.size() + 8 + bin.size()));
  appendRaw(out, (uint32_t)json.size());
  appendRaw(out, (uint32_t)0x4E4F534A);  // "JSON"
  for (char c : json) out.push_back((std::byte)c);
  appendRaw(out, (uint32_t)bin.size());
  appendRaw(out, (uint32_t)0x004E4942);  // "BIN"
  out.insert(out.end(), bin.begin(), bin.end());
  return out;
}

}  // namespace

TEST(Import, ObjCubeWithMaterialThroughResolver) {
  std::vector<std::string> asked;
  const import::Resolver resolve =
      [&](std::string_view uri) -> std::optional<std::vector<std::byte>> {
    asked.emplace_back(uri);
    if (uri == "cube.mtl") return toBytes(kCubeMtl);
    return std::nullopt;
  };
  const std::string obj = kCubeObj;
  auto model = import::model(obj.data(), obj.size(), "cube.obj", resolve);
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

TEST(Import, GltfEmbeddedBase64Buffer) {
  const std::string json = triangleGltfJson(
      "data:application/octet-stream;base64," + base64(triangleBufferBytes()));
  auto model = import::model(json.data(), json.size(), "tri.gltf");
  ASSERT_TRUE(model.has_value());
  ASSERT_EQ(model->parts.size(), 1u);
  const Part& part = model->parts.front();
  EXPECT_EQ(part.name, "tri");
  EXPECT_EQ(part.mesh.vertexCount(), 3u);
  EXPECT_EQ(part.mesh.triangleCount(), 1u);
  EXPECT_FLOAT_EQ(part.baseColor.r, 1);
  EXPECT_FLOAT_EQ(part.baseColor.b, 0);
  // Node transforms are BAKED into the positions rather than kept beside
  // them: the imported mesh is already in model space, so the unit triangle
  // under a +10 x node spans x = 10..11.
  glm::vec3 lo, hi;
  model->bounds(&lo, &hi);
  EXPECT_FLOAT_EQ(lo.x, 10);
  EXPECT_FLOAT_EQ(hi.x, 11);
}

TEST(Import, GltfExternalBufferThroughResolver) {
  const std::string json = triangleGltfJson("tri.bin");
  const import::Resolver resolve =
      [](std::string_view uri) -> std::optional<std::vector<std::byte>> {
    if (uri == "tri.bin") return triangleBufferBytes();
    return std::nullopt;
  };
  auto model = import::model(json.data(), json.size(), "tri.gltf", resolve);
  ASSERT_TRUE(model.has_value());
  EXPECT_EQ(model->triangleCount(), 1u);
  // Without the resolver the external buffer is unreachable.
  EXPECT_FALSE(import::model(json.data(), json.size(), "tri.gltf").has_value());
}

TEST(Import, GlbBinaryContainerAndSniffing) {
  const std::vector<std::byte> glb = glbBytes();
  auto model = import::model(glb.data(), glb.size(), "tri.glb");
  ASSERT_TRUE(model.has_value());
  EXPECT_EQ(model->vertexCount(), 3u);
  EXPECT_EQ(model->triangleCount(), 1u);
  // Format detection does not depend on the filename: with an extension
  // that says nothing, the leading "glTF" magic identifies the container.
  auto sniffed = import::model(glb.data(), glb.size(), "download");
  ASSERT_TRUE(sniffed.has_value());
  EXPECT_EQ(sniffed->triangleCount(), 1u);
}

TEST(Import, StlBinaryAndAscii) {
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
  auto model = import::model(stl.data(), stl.size(), "part.stl");
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
  auto text = import::model(ascii.data(), ascii.size(), "part.stl");
  ASSERT_TRUE(text.has_value());
  EXPECT_EQ(text->triangleCount(), 1u);
  EXPECT_EQ(text->parts.front().name, "tetra piece");
  EXPECT_FLOAT_EQ(text->parts.front().mesh.normals.front().z, 1);
}

// merged() flattens a multi-part model into one Mesh, and a part's material
// base colour is the only thing that would be lost by that — so it is baked
// into the vertex colour lane, per part, as the merge happens.
TEST(Import, MergedBakesBaseColorsIntoLane) {
  Model model;
  Part a;
  a.mesh = mesh::quad(2, 2);
  a.baseColor = {1, 0, 0, 1};
  Part b;
  b.mesh = mesh::quad(2, 2);
  b.baseColor = {0, 1, 0, 1};
  model.parts = {a, b};
  const Mesh merged = model.merged();
  EXPECT_EQ(merged.vertexCount(), 8u);
  ASSERT_EQ(merged.colors.size(), 8u);
  EXPECT_FLOAT_EQ(merged.colors.front().r, 1);
  EXPECT_FLOAT_EQ(merged.colors.back().g, 1);
}

TEST(Import, MergedCloudConcatenatesLanesAcrossParts) {
  // Disjoint lanes across parts: each side's values land at its own
  // offset, and the other side pads with the lane's default.
  Model model;
  Part a;
  a.mesh = mesh::quad(2, 2);
  a.scalarLanes["energy"] = {1, 2, 3, 4};
  Part b;
  b.mesh = mesh::quad(2, 2);
  b.colorLanes["heat"].assign(4, {1, 0, 0, 1});
  model.parts = {a, b};
  const Cloud merged = model.mergedCloud();
  ASSERT_EQ(merged.size(), 8u);
  const std::vector<float>* energy = merged.scalarIf("energy");
  ASSERT_TRUE(energy);
  ASSERT_EQ(energy->size(), 8u);
  EXPECT_FLOAT_EQ((*energy)[0], 1.0f);
  EXPECT_FLOAT_EQ((*energy)[3], 4.0f);
  EXPECT_FLOAT_EQ((*energy)[4], 0.0f);  // b's side pads scalar 0
  EXPECT_FLOAT_EQ((*energy)[7], 0.0f);
  const std::vector<glm::vec4>* heat = merged.colorIf("heat");
  ASSERT_TRUE(heat);
  ASSERT_EQ(heat->size(), 8u);
  EXPECT_FLOAT_EQ((*heat)[0].r, 1.0f);  // a's side pads white
  EXPECT_FLOAT_EQ((*heat)[0].g, 1.0f);
  EXPECT_FLOAT_EQ((*heat)[4].r, 1.0f);  // b's red from offset 4
  EXPECT_FLOAT_EQ((*heat)[4].g, 0.0f);
}

TEST(Import, FitTransformCentersAndScales) {
  Model model;
  Part part;
  part.mesh = mesh::quad(4, 2);
  part.mesh.transform(glm::translate(glm::mat4(1.0f), {100, 50, 0}));
  model.parts = {part};
  Mesh fitted = model.parts.front().mesh;
  fitted.transform(model.fitTransform(100));
  glm::vec3 lo, hi;
  fitted.bounds(&lo, &hi);
  // fitTransform(n) is uniform: it scales so the LARGEST extent becomes n
  // and recentres on the origin, leaving the aspect ratio alone. A
  // per-axis fit would have stretched the 4x2 quad to 100x100.
  EXPECT_NEAR(hi.x - lo.x, 100, 1e-3);
  EXPECT_NEAR(hi.y - lo.y, 50, 1e-3);
  EXPECT_NEAR(lo.x + hi.x, 0, 1e-3);  // centered
  EXPECT_NEAR(lo.y + hi.y, 0, 1e-3);
}

TEST(Import, GltfCustomAttributesBecomeLanes) {
  // glTF spells custom vertex attributes with a leading underscore. They
  // survive import as lanes named without it — "_ENERGY" becomes "ENERGY" —
  // and asCloud() carries them alongside the conventional attributes, so an
  // attribute authored in a DCC tool can drive this library directly.
  const std::string json = R"({
  "asset": {"version": "2.0"},
  "scenes": [{"nodes": [0]}], "scene": 0,
  "nodes": [{"mesh": 0, "name": "pts"}],
  "meshes": [{"primitives": [
    {"attributes": {"POSITION": 0, "_ENERGY": 1}}]}],
  "buffers": [{"byteLength": 48, "uri":
"data:application/octet-stream;base64,AAAAAAAAAAAAAAAAAACAPwAAAAAAAAAAAAAAAAAAgD8AAAAAAACAPgAAAD8AAEA/"}],
  "bufferViews": [
    {"buffer": 0, "byteOffset": 0, "byteLength": 36},
    {"buffer": 0, "byteOffset": 36, "byteLength": 12}],
  "accessors": [
    {"bufferView": 0, "componentType": 5126, "count": 3, "type": "VEC3",
     "min": [0, 0, 0], "max": [1, 1, 0]},
    {"bufferView": 1, "componentType": 5126, "count": 3,
     "type": "SCALAR"}]
})";
  auto model = import::model(json.data(), json.size(), "pts.gltf");
  ASSERT_TRUE(model.has_value());
  const import::Part& part = model->parts.front();
  ASSERT_EQ(part.mesh.vertexCount(), 3u);
  const auto energy = part.scalarLanes.find("ENERGY");
  ASSERT_NE(energy, part.scalarLanes.end());
  ASSERT_EQ(energy->second.size(), 3u);
  EXPECT_FLOAT_EQ(energy->second[0], 0.25f);
  EXPECT_FLOAT_EQ(energy->second[2], 0.75f);

  const Cloud cloud = part.asCloud();
  EXPECT_EQ(cloud.size(), 3u);
  ASSERT_TRUE(cloud.scalarIf("ENERGY"));
  EXPECT_FLOAT_EQ((*cloud.scalarIf("ENERGY"))[1], 0.5f);
}

TEST(Import, GltfVec2AndVec4CustomAttributesLandAsColorLanes) {
  // Custom attributes are routed by WIDTH: a scalar accessor becomes a
  // scalar lane, and anything wider becomes a four-component colour lane —
  // VEC2 zero-padded in z and w, VEC4 verbatim. There is no vec2 lane kind,
  // so reading .z of a VEC2 custom always gives 0, not garbage.
  std::vector<std::byte> bin;
  const float positions[9] = {0, 0, 0, 1, 0, 0, 0, 1, 0};
  for (float f : positions) appendRaw(bin, f);
  const float uv2[6] = {0.1f, 0.2f, 0.3f, 0.4f, 0.5f, 0.6f};
  for (float f : uv2) appendRaw(bin, f);
  const float wgt[12] = {1, 0, 0, 0.5f, 0, 1, 0, 0.25f, 0, 0, 1, 0.125f};
  for (float f : wgt) appendRaw(bin, f);
  const std::string json = R"({
  "asset": {"version": "2.0"},
  "scenes": [{"nodes": [0]}], "scene": 0,
  "nodes": [{"mesh": 0, "name": "pts"}],
  "meshes": [{"primitives": [
    {"attributes": {"POSITION": 0, "_UV2": 1, "_WGT": 2}}]}],
  "buffers": [{"byteLength": 108, "uri":
"data:application/octet-stream;base64,)" +
                           base64(bin) + R"("}],
  "bufferViews": [
    {"buffer": 0, "byteOffset": 0, "byteLength": 36},
    {"buffer": 0, "byteOffset": 36, "byteLength": 24},
    {"buffer": 0, "byteOffset": 60, "byteLength": 48}],
  "accessors": [
    {"bufferView": 0, "componentType": 5126, "count": 3, "type": "VEC3",
     "min": [0, 0, 0], "max": [1, 1, 0]},
    {"bufferView": 1, "componentType": 5126, "count": 3, "type": "VEC2"},
    {"bufferView": 2, "componentType": 5126, "count": 3, "type": "VEC4"}]
})";
  auto model = import::model(json.data(), json.size(), "pts.gltf");
  ASSERT_TRUE(model.has_value());
  const Part& part = model->parts.front();
  const auto uv = part.colorLanes.find("UV2");
  ASSERT_NE(uv, part.colorLanes.end());
  ASSERT_EQ(uv->second.size(), 3u);
  EXPECT_FLOAT_EQ(uv->second[1].x, 0.3f);
  EXPECT_FLOAT_EQ(uv->second[1].y, 0.4f);
  EXPECT_FLOAT_EQ(uv->second[1].z, 0.0f);  // zero-padded z/w
  EXPECT_FLOAT_EQ(uv->second[1].w, 0.0f);
  const auto wgtLane = part.colorLanes.find("WGT");
  ASSERT_NE(wgtLane, part.colorLanes.end());
  ASSERT_EQ(wgtLane->second.size(), 3u);
  EXPECT_FLOAT_EQ(wgtLane->second[0].w, 0.5f);
  EXPECT_FLOAT_EQ(wgtLane->second[2].z, 1.0f);
  EXPECT_FLOAT_EQ(wgtLane->second[2].w, 0.125f);

  const Cloud cloud = part.asCloud();
  ASSERT_TRUE(cloud.colorIf("UV2"));
  ASSERT_TRUE(cloud.colorIf("WGT"));
  EXPECT_FLOAT_EQ((*cloud.colorIf("UV2"))[2].y, 0.6f);
  EXPECT_FLOAT_EQ((*cloud.colorIf("WGT"))[1].y, 1.0f);
}

TEST(Import, PlyAttributesFlowFromAsciiAndBinary) {
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
  auto model = import::model(ascii, std::strlen(ascii), "scatter.ply");
  ASSERT_TRUE(model.has_value());
  const import::Part& part = model->parts.front();
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
  auto binModel = import::model(bin.data(), bin.size(), "tri.ply");
  ASSERT_TRUE(binModel.has_value());
  const import::Part& tri = binModel->parts.front();
  EXPECT_EQ(tri.mesh.triangleCount(), 1u);
  EXPECT_FLOAT_EQ(tri.scalarLanes.at("intensity")[2], 9.0f);
  ASSERT_EQ(tri.mesh.normals.size(), 3u);
  EXPECT_NEAR(tri.mesh.normals.front().z, 1.0f, 1e-4f);
}

TEST(Import, PlyRejectsHostileCountsAndIndices) {
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
  auto dropped = import::model(badFace, std::strlen(badFace), "bad.ply");
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
      import::model(hugeCount, std::strlen(hugeCount), "huge.ply").has_value());

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
  EXPECT_FALSE(import::model(truncated.data(), truncated.size(), "trunc.ply")
                   .has_value());
}

TEST(Import, LoneTStaysAScalarLane) {
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
  auto model = import::model(ascii, std::strlen(ascii), "lone_t.ply");
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
  const std::string bytes = save::ply(dump);
  auto trip = import::model(bytes.data(), bytes.size(), "trip.ply");
  ASSERT_TRUE(trip.has_value());
  const Cloud back = trip->parts.front().asCloud();
  ASSERT_EQ(back.size(), 3u);
  ASSERT_TRUE(back.scalarIf("t"));
  EXPECT_FLOAT_EQ((*back.scalarIf("t"))[0], 0.1f);
  EXPECT_FLOAT_EQ((*back.scalarIf("t"))[1], 0.6f);
  EXPECT_FLOAT_EQ((*back.scalarIf("t"))[2], 0.9f);
}

TEST(Import, PlyPartialSuffixTriplesStayScalarAndRgbFoldsAlphaOne) {
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
  auto model = import::model(ascii, std::strlen(ascii), "fold.ply");
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
    const std::string bytes = save::ply(quad, {.binary = binary});
    ASSERT_FALSE(bytes.empty());
    auto model = import::model(bytes.data(), bytes.size(), "prim.ply");
    ASSERT_TRUE(model.has_value());
    const import::Part& part = model->parts.front();
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

TEST(Import, PlyFacePropertiesReplicateAcrossFanTriangles) {
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
  auto model = import::model(ascii, std::strlen(ascii), "fan.ply");
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
  auto binModel = import::model(bin.data(), bin.size(), "fan.ply");
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

TEST(Import, PlyFaceLanesTakeConventionalColorAndAnyDeclaredOrder) {
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
  auto model = import::model(ascii, std::strlen(ascii), "meshlab.ply");
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

TEST(Import, PlyFaceLanesSurviveHostileFaceHeaders) {
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
  auto model = import::model(dropped, std::strlen(dropped), "drop.ply");
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
      import::model(hugeFaces, std::strlen(hugeFaces), "huge.ply").has_value());

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
  EXPECT_FALSE(import::model(shortBody, std::strlen(shortBody), "short.ply")
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
  auto dupModel = import::model(duplicate, std::strlen(duplicate), "dup.ply");
  ASSERT_TRUE(dupModel.has_value());
  const Mesh& dupMesh = dupModel->parts.front().mesh;
  ASSERT_EQ(dupMesh.triangleCount(), 1u);
  ASSERT_NE(dupMesh.primIf("heat"), nullptr);
  ASSERT_EQ(dupMesh.primIf("heat")->size(), 1u);
  EXPECT_FLOAT_EQ((*dupMesh.primIf("heat"))[0].x, 6.0f);
}

namespace {

/** An Ogawa archive built entirely in memory, so the Alembic tests need no
 *  fixture files. It contains:
 *   - "root", an xform translated +10 along x, holding "tri": a static
 *     clockwise triangle carrying a per-vertex-scope arbitrary geometry
 *     parameter;
 *   - "uvquad" and "uvweld": the SAME topology written twice, once with
 *     facevarying uvs and once with vertex-scope uvs, which are the two
 *     sides of the corner dedup;
 *   - "cloud": a top-level point cloud with two time samples at 24 fps, and
 *     ids written on the first sample only. */
std::string alembicArchiveBytes() {
  namespace Abc = Alembic::Abc;
  namespace AbcGeom = Alembic::AbcGeom;
  std::ostringstream out(std::ios::binary);
  {
    Abc::OArchive archive(
        Alembic::AbcCoreOgawa::WriteArchive()(&out, Abc::MetaData()));
    const uint32_t ts = archive.addTimeSampling(
        Alembic::AbcCoreAbstract::TimeSampling(1.0 / 24.0, 0.0));

    AbcGeom::OXform root(archive.getTop(), "root");
    AbcGeom::XformSample xs;
    xs.setTranslation(Abc::V3d(10, 0, 0));
    root.getSchema().set(xs);

    AbcGeom::OPolyMesh meshObj(root, "tri");  // static, under the xform
    const std::vector<Imath::V3f> pos = {{0, 0, 0}, {1, 0, 0}, {0, 1, 0}};
    const std::vector<int32_t> indices = {0, 2, 1};  // Alembic: clockwise
    const std::vector<int32_t> counts = {3};
    meshObj.getSchema().set(AbcGeom::OPolyMeshSchema::Sample(
        Abc::P3fArraySample(pos), Abc::Int32ArraySample(indices),
        Abc::Int32ArraySample(counts)));
    AbcGeom::OFloatGeomParam energy(meshObj.getSchema().getArbGeomParams(),
                                    "energy", false, AbcGeom::kVertexScope, 1);
    const std::vector<float> values = {0.25f, 0.5f, 0.75f};
    energy.set(AbcGeom::OFloatGeomParam::Sample(Abc::FloatArraySample(values),
                                                AbcGeom::kVertexScope));

    // Two triangles sharing the 0-2 diagonal of a unit square: 4 points and
    // 6 corners, written twice with identical topology and different uv
    // scopes so the dedup can be observed from both sides.
    const std::vector<Imath::V3f> qpos = {
        {0, 0, 0}, {1, 0, 0}, {1, 1, 0}, {0, 1, 0}};
    const std::vector<int32_t> qidx = {0, 2, 1, 0, 3, 2};  // clockwise
    const std::vector<int32_t> qcounts = {3, 3};
    // FACEVARYING: one uv per corner. Corners 1 and 5 are the same
    // point with different uvs; corners 0 and 3 are the same point with
    // the SAME uv — and still do not weld, because the key is the
    // corner INDEX, not the value.
    AbcGeom::OPolyMesh uvObj(archive.getTop(), "uvquad");
    const std::vector<Imath::V2f> quv = {{0, 0}, {1, 1}, {1, 0},
                                         {0, 0}, {0, 1}, {0.25f, 0.75f}};
    uvObj.getSchema().set(AbcGeom::OPolyMeshSchema::Sample(
        Abc::P3fArraySample(qpos), Abc::Int32ArraySample(qidx),
        Abc::Int32ArraySample(qcounts),
        AbcGeom::OV2fGeomParam::Sample(Abc::V2fArraySample(quv),
                                       AbcGeom::kFacevaryingScope)));
    // VERTEX scope: one uv per point, so every corner of a point keys
    // the same and the six corners weld back down to four vertices.
    AbcGeom::OPolyMesh weldObj(archive.getTop(), "uvweld");
    const std::vector<Imath::V2f> wuv = {{0, 0}, {1, 0}, {1, 1}, {0, 1}};
    weldObj.getSchema().set(AbcGeom::OPolyMeshSchema::Sample(
        Abc::P3fArraySample(qpos), Abc::Int32ArraySample(qidx),
        Abc::Int32ArraySample(qcounts),
        AbcGeom::OV2fGeomParam::Sample(Abc::V2fArraySample(wuv),
                                       AbcGeom::kVertexScope)));

    AbcGeom::OPoints pointsObj(archive.getTop(), "cloud", ts);  // animated
    const std::vector<uint64_t> ids = {0, 1};
    const std::vector<Imath::V3f> frame0 = {{0, 0, 0}, {1, 0, 0}};
    pointsObj.getSchema().set(AbcGeom::OPointsSchema::Sample(
        Abc::P3fArraySample(frame0), Abc::UInt64ArraySample(ids)));
    const std::vector<Imath::V3f> frame1 = {{0, 1, 0}, {1, 1, 0}};
    pointsObj.getSchema().set(
        AbcGeom::OPointsSchema::Sample(Abc::P3fArraySample(frame1)));
  }  // The OArchive destructor is what finalizes the Ogawa stream, so the
     // scope must close before .str() is read — an earlier read returns a
     // truncated, unreadable archive.
  return std::move(out).str();
}

}  // namespace

TEST(Import, AlembicMeshPointsAndLanes) {
  const std::string bytes = alembicArchiveBytes();

  // As with GLB, the filename hint carries nothing useful here and the
  // leading Ogawa magic is what routes the bytes to the Alembic reader.
  auto model = import::model(bytes.data(), bytes.size(), "download");
  ASSERT_TRUE(model.has_value());
  ASSERT_EQ(model->parts.size(), 4u);  // tri, uvquad, uvweld, cloud
  const auto find = [&](std::string_view name) -> const Part* {
    for (const Part& part : model->parts)
      if (part.name == name) return &part;
    return nullptr;
  };
  const Part* tri = find("tri");
  const Part* cloud = find("cloud");
  ASSERT_NE(tri, nullptr);
  ASSERT_NE(cloud, nullptr);

  EXPECT_EQ(tri->mesh.triangleCount(), 1u);
  glm::vec3 lo, hi;
  tri->mesh.bounds(&lo, &hi);
  // Parent xforms are baked into the positions, as glTF node transforms are.
  EXPECT_FLOAT_EQ(lo.x, 10.0f);
  EXPECT_FLOAT_EQ(hi.x, 11.0f);
  // WINDING: Alembic faces are wound clockwise, this library's meshes
  // counter-clockwise, so the importer reverses each face. The fixture's
  // triangle therefore ends up with its derived normal along +z; leaving
  // the file's order alone would point it at -z and cull the whole model.
  ASSERT_EQ(tri->mesh.normals.size(), 3u);
  EXPECT_GT(tri->mesh.normals[0].z, 0.0f);
  // A vertex-scope arbitrary geometry parameter becomes a per-point scalar
  // lane, and each value has to follow its point through the corner dedup
  // and the winding reversal — both of which renumber vertices. Checked by
  // recomputing the value from the position it ended up on (the fixture's
  // values are 0.25 + 0.25x + 0.5y in local space) rather than by index,
  // since matching by index would pass even if the lane were shuffled.
  const auto energy = tri->scalarLanes.find("energy");
  ASSERT_NE(energy, tri->scalarLanes.end());
  ASSERT_EQ(energy->second.size(), 3u);
  for (size_t i = 0; i < 3; ++i) {
    const glm::vec3 local = tri->mesh.positions[i] - glm::vec3{10, 0, 0};
    EXPECT_NEAR(energy->second[i], 0.25f + 0.25f * local.x + 0.5f * local.y,
                1e-6f);
  }
  EXPECT_EQ(tri->asCloud().scalars.count("energy"), 1u);

  // An OPoints object imports as a part with no indices, its ids carried as
  // an ordinary scalar lane. With no time requested, the first sample is
  // the one read.
  EXPECT_TRUE(cloud->mesh.indices.empty());
  ASSERT_EQ(cloud->mesh.positions.size(), 2u);
  EXPECT_FLOAT_EQ(cloud->mesh.positions[0].y, 0.0f);
  const auto id = cloud->scalarLanes.find("id");
  ASSERT_NE(id, cloud->scalarLanes.end());
  ASSERT_EQ(id->second.size(), 2u);
  EXPECT_FLOAT_EQ(id->second[0], 0.0f);
  EXPECT_FLOAT_EQ(id->second[1], 1.0f);

  // Malformed input returns an empty optional rather than throwing or
  // aborting: the Alembic library signals errors by exception, and those
  // must not escape into a caller that is merely opening an untrusted file.
  const char garbage[] = "not an alembic archive at all";
  EXPECT_FALSE(import::alembic(garbage, sizeof(garbage)).has_value());
  EXPECT_FALSE(import::alembic(bytes.data(), bytes.size() / 2).has_value());
}

TEST(Import, AlembicFacevaryingUvsFlipAndDedup) {
  // Two conventions, one fixture.
  //
  // (1) UV ORIGIN: Alembic's is BOTTOM-left, this library's Mesh uses
  // top-left, so v is flipped on import. Get it wrong and every textured
  // Alembic model is upside down.
  //
  // (2) DEDUP: corners merge OBJ-style on (position, uv, normal) — corners
  // that agree on all three become one vertex, and a disagreement in uv
  // splits one point into two vertices so each can keep its own uv.
  const std::string bytes = alembicArchiveBytes();
  auto model = import::alembic(bytes.data(), bytes.size());
  ASSERT_TRUE(model.has_value());
  const auto part = [&](std::string_view name) -> const Part* {
    for (const Part& p : model->parts)
      if (p.name == name) return &p;
    return nullptr;
  };
  // uvs indexed by POSITION — insertion order is a detail of the walk.
  const auto uvsAt = [](const Mesh& m, glm::vec3 p) {
    std::vector<glm::vec2> found;
    for (size_t i = 0; i < m.positions.size(); ++i)
      if (glm::length(m.positions[i] - p) < 1e-6f) found.push_back(m.uvs[i]);
    std::sort(found.begin(), found.end(),
              [](glm::vec2 a, glm::vec2 b) { return a.x < b.x; });
    return found;
  };

  // --- FACEVARYING: the uv source is the CORNER, and the dedup keys on the
  // corner index rather than on the uv value, so every corner becomes its
  // own vertex — 6 corners give 6 vertices even where two of them happen to
  // carry an identical uv. Nothing downstream depends on the merge, so the
  // reader does not pay for comparing values.
  const Part* quad = part("uvquad");
  ASSERT_NE(quad, nullptr);
  const Mesh& fv = quad->mesh;
  ASSERT_EQ(fv.positions.size(), 6u);
  ASSERT_EQ(fv.uvs.size(), 6u);
  EXPECT_EQ(fv.triangleCount(), 2u);
  // The flip in one value: the file's uv (0,0) on the origin corner arrives
  // as (0,1). No reader that passed v through unchanged could produce that.
  const std::vector<glm::vec2> atOrigin = uvsAt(fv, {0, 0, 0});
  ASSERT_EQ(atOrigin.size(), 2u);  // corners 0 and 3, NOT welded
  for (const glm::vec2& uv : atOrigin) {
    EXPECT_FLOAT_EQ(uv.x, 0.0f);
    EXPECT_FLOAT_EQ(uv.y, 1.0f);
  }
  EXPECT_FLOAT_EQ(uvsAt(fv, {0, 1, 0}).at(0).y, 0.0f);  // file v=1 -> 0
  // The shared point at (1,1,0) carried two different uvs; both survive,
  // both flipped.
  const std::vector<glm::vec2> shared = uvsAt(fv, {1, 1, 0});
  ASSERT_EQ(shared.size(), 2u);
  EXPECT_FLOAT_EQ(shared[0].x, 0.25f);
  EXPECT_FLOAT_EQ(shared[0].y, 0.25f);  // file (0.25, 0.75)
  EXPECT_FLOAT_EQ(shared[1].x, 1.0f);
  EXPECT_FLOAT_EQ(shared[1].y, 0.0f);  // file (1, 1)

  // --- VERTEX scope: the uv source IS the point, so every corner of a
  // point keys the same and the six corners weld back down to four
  // vertices. Identical topology to the facevarying case, identical flip —
  // only the scope differs.
  const Part* weld = part("uvweld");
  ASSERT_NE(weld, nullptr);
  const Mesh& vw = weld->mesh;
  ASSERT_EQ(vw.positions.size(), 4u);  // the merge the facevarying case
  ASSERT_EQ(vw.uvs.size(), 4u);        // cannot make
  EXPECT_EQ(vw.triangleCount(), 2u);
  const std::vector<glm::vec2> weldOrigin = uvsAt(vw, {0, 0, 0});
  ASSERT_EQ(weldOrigin.size(), 1u);
  EXPECT_FLOAT_EQ(weldOrigin[0].x, 0.0f);
  EXPECT_FLOAT_EQ(weldOrigin[0].y, 1.0f);  // file (0,0) -> (0,1)
  const std::vector<glm::vec2> weldShared = uvsAt(vw, {1, 1, 0});
  ASSERT_EQ(weldShared.size(), 1u);
  EXPECT_FLOAT_EQ(weldShared[0].x, 1.0f);
  EXPECT_FLOAT_EQ(weldShared[0].y, 0.0f);  // file (1,1) -> (1,0)
}

TEST(Import, AlembicTimeSampleSelection) {
  const std::string bytes = alembicArchiveBytes();
  const auto cloudY = [&](double time) -> float {
    auto model = import::alembic(bytes.data(), bytes.size(), {.time = time});
    if (!model) return -1.0f;
    for (const Part& part : model->parts)
      if (part.name == "cloud") return part.mesh.positions.at(0).y;
    return -1.0f;
  };
  // A requested time selects the NEAREST stored sample, not the one before
  // it: 0.6 of a frame in is closer to frame 1, so frame 1 is what is read.
  // Samples are never interpolated, so the values are always ones an
  // authoring tool actually wrote.
  EXPECT_FLOAT_EQ(cloudY(0), 0.0f);
  EXPECT_FLOAT_EQ(cloudY(1.0 / 24), 1.0f);
  EXPECT_FLOAT_EQ(cloudY(0.6 / 24), 1.0f);
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

  const std::string bytes = save::ply(cloud);
  auto model = import::model(bytes.data(), bytes.size(), "trip.ply");
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
  const std::string bytes = save::ply(quad);
  auto model = import::model(bytes.data(), bytes.size(), "quad.ply");
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

  const std::string bytes = save::ply(cloud, {.binary = true});
  auto model = import::model(bytes.data(), bytes.size(), "bin.ply");
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
  const std::string meshBytes = save::ply(quad, {.binary = true});
  auto meshModel =
      import::model(meshBytes.data(), meshBytes.size(), "quad.ply");
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
  const std::string bytes = save::ply(cloud);
  auto model = import::model(bytes.data(), bytes.size(), "skip.ply");
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
  EXPECT_TRUE(save::ply(Cloud{}).empty());
  EXPECT_TRUE(save::ply(Mesh{}).empty());
  const std::filesystem::path file = std::filesystem::temp_directory_path() /
                                     "sigilgeometry_empty_decline.ply";
  EXPECT_FALSE(save::ply(file, Cloud{}));
  EXPECT_FALSE(save::ply(file, Mesh{}));
}

TEST(Save, PlyWritesPrimLanesAsFaceProperties) {
  Mesh m = splitQuad();
  m.prim("Color")[0] = {1, 0, 0, 1};
  m.prim("Color")[1] = {0, 0.25f, 0, 1};
  const std::string text = save::ply(m);
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
  auto back = import::model(text.data(), text.size(), "prims.ply");
  ASSERT_TRUE(back.has_value());
  ASSERT_EQ(back->parts.size(), 1u);
  EXPECT_EQ(back->parts.front().mesh.triangleCount(), 2u);
}

TEST(Import, HoudiniGeoPolygonsUnweldWithVertexAndPrimitiveClasses) {
  // A quad and a triangle over five points, written the way Houdini
  // saves ASCII .geo: alternating key/value arrays, paged attribute
  // storage for P, a plain tuple list for a point N, a VERTEX uv (which
  // outranks any point uv), a primitive Cd, a point group and a
  // primitive group. Points 0-3 are the quad (y = 0 and y = 100),
  // point 4 sits above and forms a triangle with points 2 and 3.
  const char* geo = R"([
    "fileversion","20.5.278",
    "hasindex",false,
    "pointcount",5,
    "vertexcount",7,
    "primitivecount",2,
    "info",{"software":"Houdini 20.5.278"},
    "topology",["pointref",["indices",[0,1,2,3,3,2,4]]],
    "attributes",[
      "vertexattributes",[
        [
          ["scope","public","type","numeric","name","uv","options",{}],
          ["size",3,"storage","fpreal32","defaults",["size",1,"storage","fpreal64","values",[0]],
           "values",["size",3,"storage","fpreal32","tuples",
             [[0,0,0],[1,0,0],[1,1,0],[0,1,0],[0,1,0],[1,1,0],[0.5,1,0]]]]
        ]
      ],
      "pointattributes",[
        [
          ["scope","public","type","numeric","name","P","options",{"type":{"type":"string","value":"point"}}],
          ["size",3,"storage","fpreal32","defaults",["size",1,"storage","fpreal64","values",[0]],
           "values",["size",3,"storage","fpreal32","packing",[3],"pagesize",1024,
             "constantpageflags",[[false]],
             "rawpagedata",[0,0,0, 100,0,0, 100,100,0, 0,100,0, 50,180,0]]]
        ],
        [
          ["scope","public","type","numeric","name","N","options",{"type":{"type":"string","value":"normal"}}],
          ["size",3,"storage","fpreal32","defaults",["size",1,"storage","fpreal64","values",[0]],
           "values",["size",3,"storage","fpreal32","tuples",
             [[0,0,1],[0,0,1],[0,0,1],[0,0,1],[0,0,1]]]]
        ],
        [
          ["scope","public","type","numeric","name","pscale","options",{}],
          ["size",1,"storage","fpreal32","defaults",["size",1,"storage","fpreal64","values",[1]],
           "values",["size",1,"storage","fpreal32","arrays",[[1,2,3,4,5]]]]
        ]
      ],
      "primitiveattributes",[
        [
          ["scope","public","type","numeric","name","Cd","options",{"type":{"type":"string","value":"color"}}],
          ["size",3,"storage","fpreal32","defaults",["size",1,"storage","fpreal64","values",[1]],
           "values",["size",3,"storage","fpreal32","tuples",[[1,0,0],[0,0,1]]]]
        ]
      ],
      "globalattributes",[
        [
          ["scope","public","type","numeric","name","frame","options",{}],
          ["size",1,"storage","fpreal32","defaults",["size",1,"storage","fpreal64","values",[0]],
           "values",["size",1,"storage","fpreal32","arrays",[[12]]]]
        ]
      ]
    ],
    "primitives",[
      [["type","Polygon"],["vertex",[0,1,2,3],"closed",true]],
      [["type","Polygon"],["vertex",[4,5,6],"closed",true]]
    ],
    "pointgroups",[
      [["name","top"],["selection",["unordered",["boolRLE",[2,false,2,true,1,true]]]]]
    ],
    "primitivegroups",[
      [["name","front"],["selection",["unordered",["i8",[1,0]]]]]
    ]
  ])";
  const std::optional<import::Model> model =
      import::model(geo, std::strlen(geo), "scene.geo");
  ASSERT_TRUE(model);
  ASSERT_EQ(model->parts.size(), 1u);
  const import::Part& part = model->parts.front();
  const Mesh& mesh = part.mesh;
  // Unwelded: 4 + 3 vertices; the quad fans into two triangles.
  EXPECT_EQ(mesh.vertexCount(), 7u);
  EXPECT_EQ(mesh.triangleCount(), 3u);
  EXPECT_EQ(mesh.positions[2].x, 100.0f);
  EXPECT_EQ(mesh.positions[2].y, 100.0f);
  EXPECT_EQ(mesh.positions[6].y, 180.0f);  // vertex 6 -> point 4
  ASSERT_EQ(mesh.normals.size(), 7u);
  EXPECT_FLOAT_EQ(mesh.normals[0].z, 1.0f);
  ASSERT_EQ(mesh.uvs.size(), 7u);
  // Vertex uv, v flipped to the top-left convention.
  EXPECT_FLOAT_EQ(mesh.uvs[2].x, 1.0f);
  EXPECT_FLOAT_EQ(mesh.uvs[2].y, 0.0f);
  EXPECT_FLOAT_EQ(mesh.uvs[6].x, 0.5f);
  // Primitive Cd -> the "Color" prim lane, replicated over the fan.
  const std::vector<glm::vec4>* color = mesh.primIf("Color");
  ASSERT_TRUE(color);
  ASSERT_EQ(color->size(), 3u);
  EXPECT_FLOAT_EQ((*color)[0].r, 1.0f);
  EXPECT_FLOAT_EQ((*color)[1].r, 1.0f);
  EXPECT_FLOAT_EQ((*color)[2].b, 1.0f);
  // Primitive group -> a 0/1 prim lane.
  const std::vector<glm::vec4>* front = mesh.primIf("front");
  ASSERT_TRUE(front);
  EXPECT_FLOAT_EQ((*front)[0].x, 1.0f);
  EXPECT_FLOAT_EQ((*front)[1].x, 1.0f);
  EXPECT_FLOAT_EQ((*front)[2].x, 0.0f);
  // Point attributes ride to the Part through the owning point;
  // point groups are 0/1 scalar lanes.
  const auto pscale = part.scalarLanes.find("pscale");
  ASSERT_NE(pscale, part.scalarLanes.end());
  ASSERT_EQ(pscale->second.size(), 7u);
  EXPECT_FLOAT_EQ(pscale->second[6], 5.0f);
  const auto top = part.scalarLanes.find("top");
  ASSERT_NE(top, part.scalarLanes.end());
  EXPECT_FLOAT_EQ(top->second[0], 0.0f);
  EXPECT_FLOAT_EQ(top->second[2], 1.0f);
  EXPECT_FLOAT_EQ(top->second[6], 1.0f);
  // Sniffed without an extension too.
  EXPECT_TRUE(import::model(geo, std::strlen(geo), ""));
  // ...and it feeds the pop system through asCloud like any import.
  const Cloud cloud = part.asCloud();
  EXPECT_EQ(cloud.size(), 7u);
  EXPECT_TRUE(cloud.scalarIf("top"));
}

TEST(Import, HoudiniGeoPointsBecomeAHonestCloud) {
  // No primitives: a particle-style file. P in a paged layout whose
  // second page is CONSTANT (every point on it shares one tuple), an
  // int id, a float4 orient, a string name (kept out of the lanes), and
  // a Cd point colour that lands on the mesh colour lane.
  const char* geo = R"([
    "fileversion","20.5.278",
    "pointcount",6,
    "vertexcount",0,
    "primitivecount",0,
    "topology",["pointref",["indices",[]]],
    "attributes",[
      "pointattributes",[
        [
          ["scope","public","type","numeric","name","P","options",{}],
          ["size",3,"storage","fpreal32",
           "values",["size",3,"storage","fpreal32","packing",[3],"pagesize",4,
             "constantpageflags",[[false,true]],
             "rawpagedata",[0,0,0, 1,0,0, 2,0,0, 3,0,0,  9,9,9]]]
        ],
        [
          ["scope","public","type","numeric","name","id","options",{}],
          ["size",1,"storage","int32",
           "values",["size",1,"storage","int32","arrays",[[10,11,12,13,14,15]]]]
        ],
        [
          ["scope","public","type","numeric","name","orient","options",{}],
          ["size",4,"storage","fpreal32",
           "values",["size",4,"storage","fpreal32","tuples",
             [[0,0,0,1],[0,0,0,1],[0,0,0,1],[0,0,0,1],[0,0,0,1],[0,0,0,1]]]]
        ],
        [
          ["scope","public","type","numeric","name","Cd","options",{}],
          ["size",3,"storage","fpreal32",
           "values",["size",3,"storage","fpreal32","packing",[1,1,1],"pagesize",8,
             "constantpageflags",[[true],[true],[false]],
             "rawpagedata",[0.5, 0.25, 0,0.2,0.4,0.6,0.8,1.0]]]
        ],
        [
          ["scope","public","type","string","name","name","options",{}],
          ["size",1,"storage","int32","strings",["a","b"],
           "indices",["size",1,"storage","int32","arrays",[[0,1,0,1,0,1]]]]
        ]
      ]
    ],
    "primitives",[]
  ])";
  const std::optional<import::Model> model =
      import::model(geo, std::strlen(geo), "particles.geo");
  ASSERT_TRUE(model);
  const import::Part& part = model->parts.front();
  const Mesh& mesh = part.mesh;
  EXPECT_EQ(mesh.vertexCount(), 6u);
  EXPECT_EQ(mesh.triangleCount(), 0u);
  EXPECT_FLOAT_EQ(mesh.positions[3].x, 3.0f);
  // The constant page: points 4 and 5 both read the one tuple.
  EXPECT_FLOAT_EQ(mesh.positions[4].x, 9.0f);
  EXPECT_FLOAT_EQ(mesh.positions[5].z, 9.0f);
  // Split packing [1,1,1]: R and G constant pages, B a full page.
  ASSERT_EQ(mesh.colors.size(), 6u);
  EXPECT_FLOAT_EQ(mesh.colors[0].r, 0.5f);
  EXPECT_FLOAT_EQ(mesh.colors[5].g, 0.25f);
  EXPECT_FLOAT_EQ(mesh.colors[2].b, 0.4f);
  EXPECT_FLOAT_EQ(mesh.colors[5].b, 1.0f);
  const auto id = part.scalarLanes.find("id");
  ASSERT_NE(id, part.scalarLanes.end());
  EXPECT_FLOAT_EQ(id->second[5], 15.0f);
  const auto orient = part.colorLanes.find("orient");
  ASSERT_NE(orient, part.colorLanes.end());
  EXPECT_FLOAT_EQ(orient->second[0].w, 1.0f);
  EXPECT_EQ(part.scalarLanes.count("name"), 0u);
  const Cloud cloud = part.asCloud();
  EXPECT_EQ(cloud.size(), 6u);
  EXPECT_TRUE(cloud.colorIf("tint"));
  EXPECT_TRUE(cloud.scalarIf("id"));
}

TEST(Import, GltfCarriesTheWholeMaterial) {
  // The fetched Khronos Avocado (skipped when the asset is absent):
  // base colour, a normal map and a packed metallicRoughness image,
  // with the occlusion slot naming the same bytes as the pack.
  const std::filesystem::path glb = "assets/models/Avocado.glb";
  std::filesystem::path found;
  for (const std::filesystem::path candidate :
       {glb, std::filesystem::path("build") / glb,
        std::filesystem::path("../build") / glb,
        std::filesystem::path("../../build") / glb})
    if (std::filesystem::exists(candidate)) found = candidate;
  if (found.empty()) GTEST_SKIP() << "Avocado.glb not fetched";
  const std::optional<import::Model> model = import::model(found);
  ASSERT_TRUE(model);
  ASSERT_FALSE(model->parts.empty());
  const import::Part& part = model->parts.front();
  EXPECT_FALSE(part.textureBytes.empty());
  ASSERT_TRUE(part.textures.count("normal"));
  ASSERT_TRUE(part.textures.count("orm"));
  EXPECT_FALSE(part.textures.at("normal").bytes.empty());
  EXPECT_FALSE(part.textures.at("orm").bytes.empty());
  EXPECT_FLOAT_EQ(part.metallic, 1.0f);
  EXPECT_FLOAT_EQ(part.roughness, 1.0f);
  if (part.textures.count("occlusion"))
    EXPECT_EQ(part.textures.at("occlusion").bytes,
              part.textures.at("orm").bytes)
        << "the Avocado packs occlusion into the same image";
}

TEST(Import, MaterialSlotsRideThePrimitiveClass) {
  // A .geo with a string shop_materialpath per primitive lands as the
  // "Material" prim lane by string-table index; the fetched Avocado (one
  // material) names slot 0 and merged() keeps the lane.
  const char* geo = R"([
    "fileversion","20.5.278","pointcount",4,"vertexcount",6,"primitivecount",2,
    "topology",["pointref",["indices",[0,1,2,0,2,3]]],
    "attributes",["pointattributes",[
      [["scope","public","type","numeric","name","P","options",{}],
       ["size",3,"storage","fpreal32","values",["size",3,"storage","fpreal32",
        "tuples",[[0,0,0],[1,0,0],[1,1,0],[0,1,0]]]]]],
     "primitiveattributes",[
      [["scope","public","type","string","name","shop_materialpath","options",{}],
       ["size",1,"storage","int32","strings",["/mat/steel","/mat/glass"],
        "indices",["size",1,"storage","int32","arrays",[[1,0]]]]]]],
    "primitives",[[["type","Polygon"],["vertex",[0,1,2],"closed",true]],
                  [["type","Polygon"],["vertex",[3,4,5],"closed",true]]]
  ])";
  const std::optional<import::Model> model =
      import::model(geo, std::strlen(geo), "slots.geo");
  ASSERT_TRUE(model);
  const std::vector<glm::vec4>* lane =
      model->parts.front().mesh.primIf("Material");
  ASSERT_TRUE(lane);
  ASSERT_EQ(lane->size(), 2u);
  EXPECT_FLOAT_EQ((*lane)[0].x, 1.0f);  // "/mat/glass"
  EXPECT_FLOAT_EQ((*lane)[1].x, 0.0f);  // "/mat/steel"

  std::filesystem::path found;
  for (const std::filesystem::path candidate :
       {std::filesystem::path("assets/models/Avocado.glb"),
        std::filesystem::path("build/assets/models/Avocado.glb"),
        std::filesystem::path("../build/assets/models/Avocado.glb"),
        std::filesystem::path("../../build/assets/models/Avocado.glb")})
    if (std::filesystem::exists(candidate)) found = candidate;
  if (found.empty()) return;  // the .geo half already stands
  const std::optional<import::Model> avocado = import::model(found);
  ASSERT_TRUE(avocado);
  EXPECT_EQ(avocado->materialSlotCount(), 1);
  EXPECT_EQ(avocado->parts.front().materialIndex, 0);
  const Mesh merged = avocado->merged();
  const std::vector<glm::vec4>* slots = merged.primIf("Material");
  ASSERT_TRUE(slots);
  EXPECT_EQ(slots->size(), merged.triangleCount());
}
