/** @file
 * The glTF reader: an embedded base64 buffer, an external one fetched
 * through the resolver, the GLB container recognised from its own bytes,
 * the custom attributes that become lanes at the width they were declared
 * at, and the whole material a primitive names.
 */

#include <gtest/gtest.h>

#include <cmath>
#include <cstring>
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
using sigil::geometry::test::toBytes;

namespace {

using codec::decode::Model;
using codec::decode::Part;

std::string base64(const std::vector<std::byte>& bytes) {
  static const char* alphabet =
      "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
  std::string out;
  for (size_t i = 0; i < bytes.size(); i += 3) {
    uint32_t chunk = (uint32_t)bytes[i] << 16u;
    if (i + 1 < bytes.size()) chunk |= (uint32_t)bytes[i + 1] << 8u;
    if (i + 2 < bytes.size()) chunk |= (uint32_t)bytes[i + 2];
    out.push_back(alphabet[(chunk >> 18u) & 63u]);
    out.push_back(alphabet[(chunk >> 12u) & 63u]);
    out.push_back(i + 1 < bytes.size() ? alphabet[(chunk >> 6u) & 63u] : '=');
    out.push_back(i + 2 < bytes.size() ? alphabet[chunk & 63u] : '=');
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
  buffer += '}';
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

TEST(ReadGltf, AnEmbeddedBase64BufferIsDecodedInPlace) {
  const std::string json = triangleGltfJson(
      "data:application/octet-stream;base64," + base64(triangleBufferBytes()));
  auto model = codec::decode::model(json.data(), json.size(), "tri.gltf");
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

TEST(ReadGltf, AnExternalBufferIsFetchedThroughTheResolver) {
  const std::string json = triangleGltfJson("tri.bin");
  const codec::decode::Resolver resolve =
      [](std::string_view uri) -> std::optional<std::vector<std::byte>> {
    if (uri == "tri.bin") return triangleBufferBytes();
    return std::nullopt;
  };
  auto model =
      codec::decode::model(json.data(), json.size(), "tri.gltf", resolve);
  ASSERT_TRUE(model.has_value());
  EXPECT_EQ(model->triangleCount(), 1u);
  // Without the resolver the external buffer is unreachable.
  EXPECT_FALSE(
      codec::decode::model(json.data(), json.size(), "tri.gltf").has_value());
}

TEST(ReadGltf, TheGlbContainerIsRecognisedFromItsOwnBytes) {
  const std::vector<std::byte> glb = glbBytes();
  auto model = codec::decode::model(glb.data(), glb.size(), "tri.glb");
  ASSERT_TRUE(model.has_value());
  EXPECT_EQ(model->vertexCount(), 3u);
  EXPECT_EQ(model->triangleCount(), 1u);
  // Format detection does not depend on the filename: with an extension
  // that says nothing, the leading "glTF" magic identifies the container.
  auto sniffed = codec::decode::model(glb.data(), glb.size(), "download");
  ASSERT_TRUE(sniffed.has_value());
  EXPECT_EQ(sniffed->triangleCount(), 1u);
}

TEST(ReadGltf, CustomAttributesBecomeLanesUnderTheirOwnNames) {
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
  auto model = codec::decode::model(json.data(), json.size(), "pts.gltf");
  ASSERT_TRUE(model.has_value());
  const codec::decode::Part& part = model->parts.front();
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

TEST(ReadGltf, TwoAndFourWideCustomAttributesLandAsColourLanes) {
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
  auto model = codec::decode::model(json.data(), json.size(), "pts.gltf");
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

TEST(ReadGltf, APrimitiveCarriesTheWholeMaterialItNames) {
  // The fetched Khronos Avocado (skipped when the asset is absent):
  // base colour, a normal map and a packed metallicRoughness image,
  // with the occlusion slot naming the same bytes as the pack.
  const std::filesystem::path glb = "assets/models/Avocado.glb";
  std::filesystem::path found;
  for (const std::filesystem::path& candidate :
       {glb, std::filesystem::path("build") / glb,
        std::filesystem::path("../build") / glb,
        std::filesystem::path("../../build") / glb})
    if (std::filesystem::exists(candidate)) found = candidate;
  if (found.empty()) GTEST_SKIP() << "Avocado.glb not fetched";
  const std::optional<codec::decode::Model> model = codec::decode::model(found);
  ASSERT_TRUE(model);
  ASSERT_FALSE(model->parts.empty());
  const codec::decode::Part& part = model->parts.front();
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
