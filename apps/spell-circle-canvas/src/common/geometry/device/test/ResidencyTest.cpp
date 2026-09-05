/** @file
 * What the device keeps between draws: the depth of a map's chain, the
 * buffers a named mesh is uploaded into once and drawn from after, the
 * streaming pair a mesh nobody can name is written through, and the
 * letting go that keeps a scene from holding everything it ever cooked.
 */

#include <gtest/gtest.h>
#include <sigilgeometry/device/Meshes.h>
#include <sigilgeometry/device/Textures.h>

#include <Graphics/GraphicsEngine/interface/Buffer.h>

#include "OnDevice.h"

using namespace sigil::geometry;

namespace {

/** Two triangles, enough to have vertices and indices and nothing more. */
mesh::Mesh quad() {
  mesh::Mesh out;
  out.positions = {{0, 0, 0}, {1, 0, 0}, {1, 1, 0}, {0, 1, 0}};
  out.indices = {0, 1, 2, 0, 2, 3};
  return out;
}

mesh::Mesh triangle() {
  mesh::Mesh out;
  out.positions = {{0, 0, 0}, {1, 0, 0}, {1, 1, 0}};
  out.indices = {0, 1, 2};
  return out;
}

}  // namespace

TEST(MapUpload, AMapTooSmallToHalveAsksForNoChainAtAll) {
  // A map wider than one texel wears the whole chain, because a surface
  // smaller on screen than its map is in texels aliases without one: the
  // count is how many times the wider side can be halved before it
  // arrives at one, and the last one is the whole map.
  EXPECT_EQ(device::mapMipLevels(8, 8), 4);
  EXPECT_EQ(device::mapMipLevels(16, 4), 5);
  // …and a ONE-TEXEL map — how a constant slot such as an emissive tint
  // is spelled — has one level and asks for nothing under it, since
  // halving a single texel arrives nowhere and a device told to fill the
  // levels below a view with one level in it refuses.
  EXPECT_EQ(device::mapMipLevels(1, 1), 1);
  EXPECT_EQ(device::mapMipLevels(2, 1), 2);
}

TEST(MeshResidency, LayoutStepsAWholeVertexWithOrWithoutThePrimitiveLane) {
  // Every vertex carries the primitive lane because it is the same
  // buffer; a program that does not read it declares four elements over
  // the same stride rather than a packing of its own.
  EXPECT_EQ(device::meshLayout(/*withPrimitiveLane=*/false).size(), 4u);
  EXPECT_EQ(device::meshLayout(/*withPrimitiveLane=*/true).size(), 5u);
  for (const Diligent::LayoutElement& element : device::meshLayout(true))
    EXPECT_EQ(element.Stride, sizeof(device::MeshVertex));
}

TEST(MeshResidency, UploadsAMeshOnceUnderTheNameItsArtefactWasGiven) {
  SIGIL_ON_DEVICE_OR_SKIP(on);
  device::MeshResidency resident(*on);

  const device::MeshBuffers* first = resident.upload(7, quad());
  ASSERT_NE(first, nullptr);
  EXPECT_EQ(first->vertexCount, 4u);
  EXPECT_EQ(first->indexCount, 6u);

  // The SAME buffers on the second ask: a number naming an artefact is
  // what says two frames are looking at the same triangles, so nothing
  // crosses to the device a second time.
  Diligent::IBuffer* vertices = first->vertices;
  const device::MeshBuffers* again = resident.upload(7, quad());
  ASSERT_NE(again, nullptr);
  EXPECT_EQ(again->vertices.RawPtr(), vertices);
}

TEST(MeshResidency, LetsGoOfAMeshNoDrawHasNamedLately) {
  SIGIL_ON_DEVICE_OR_SKIP(on);
  device::MeshResidency resident(*on);
  ASSERT_NE(resident.upload(3, quad()), nullptr);

  // Frames pass and nothing asks for it again, so the buffers go; the
  // next ask uploads afresh rather than answering with what a scene
  // stopped drawing long ago.
  for (int i = 0; i < 8; ++i) resident.endFrame();
  const device::MeshBuffers* again = resident.upload(3, triangle());
  ASSERT_NE(again, nullptr);
  EXPECT_EQ(again->vertexCount, 3u);
  EXPECT_EQ(again->indexCount, 3u);
}

TEST(MeshResidency, StreamsAMeshNobodyCanName) {
  SIGIL_ON_DEVICE_OR_SKIP(on);
  device::MeshResidency resident(*on);

  const device::MeshBuffers* wide = resident.stream(quad());
  ASSERT_NE(wide, nullptr);
  EXPECT_EQ(wide->vertexCount, 4u);
  Diligent::IBuffer* vertices = wide->vertices;

  // GROWN, NEVER SHRUNK: a smaller mesh after a larger one writes into
  // the buffer that is already big enough, so a stream that has settled
  // allocates nothing and the counts are this draw's, not the buffers'.
  const device::MeshBuffers* narrow = resident.stream(triangle());
  ASSERT_NE(narrow, nullptr);
  EXPECT_EQ(narrow->vertexCount, 3u);
  EXPECT_EQ(narrow->indexCount, 3u);
  EXPECT_EQ(narrow->vertices, vertices);
}

TEST(MeshResidency, RefusesAMeshWithNoTriangles) {
  SIGIL_ON_DEVICE_OR_SKIP(on);
  device::MeshResidency resident(*on);
  EXPECT_EQ(resident.upload(1, mesh::Mesh{}), nullptr);
  EXPECT_EQ(resident.stream(mesh::Mesh{}), nullptr);
}

TEST(TextureResidency, ReadsNothingFromAMapThatYieldsNoImage) {
  SIGIL_ON_DEVICE_OR_SKIP(on);
  device::TextureResidency resident(*on);
  EXPECT_EQ(resident.sample(sigil::material::Texture{}), nullptr);
  EXPECT_EQ(resident.environment(sigil::material::EnvironmentMap{}), nullptr);
  EXPECT_EQ(resident.irradiance(sigil::material::EnvironmentMap{}), nullptr);
}
