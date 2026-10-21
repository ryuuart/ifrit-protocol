#pragma once

/** @file
 * @ingroup world-frame
 * What a pass is allowed to see: the bodies a frame extracted, the
 * lights, the environment, the camera and the extent. It is read-only and it is
 * the ONLY door onto the scene an execution has — the description tree is not
 * reachable from a pass.
 */

#include <sigilgeometry/mesh/Instances.h>
#include <sigilgeometry/mesh/Mesh.h>
#include <sigilgeometry/mesh/camera/Camera.h>
#include <sigilmaterial/core/Backface.h>
#include <sigilmaterial/texture/Texture.h>
#include <sigilmedia/core/Picture.h>
#include <sigilworld/advanced/Light.h>
#include <sigilworld/advanced/Node.h>
#include <sigilworld/frame/Environment.h>
#include <sigilworld/node/Collection.h>

#include <chrono>
#include <glm/mat3x3.hpp>
#include <glm/mat4x4.hpp>
#include <glm/vec2.hpp>
#include <glm/vec4.hpp>
#include <span>
#include <string>
#include <string_view>

namespace sigil::world {

/** ONE BODY, as an execution reads it: where it stands, what triangles
 *  it is, what it is painted with, and what a collection asks about.
 *  Every pointer and span here addresses state the frame retains, and
 *  stands for as long as the view does. */
struct Draw {
  glm::mat4 world{1.0f};
  const geometry::mesh::Mesh* mesh = nullptr;
  /** THE INSTANCES the mesh is repeated at, each placed within `world`,
   *  when the body is instanced geometry — the mesh is then its stamp —
   *  and null for a body drawn once. */
  const geometry::mesh::Instances* instances = nullptr;
  /** Whether each instance turns to face the camera, keeping where it
   *  stands and its own scale. A body drawn once is turned as a whole
   *  before it arrives here, and says `Facing::None`. */
  Facing facing = Facing::None;
  /** Whether reverse-wound faces survive rasterization. */
  material::Backface backface = material::Backface::Hidden;
  /** The primitive lane whose colour multiplies each triangle, or empty
   *  for none. */
  std::string_view primitiveColor;
  /** WHICH COOKED ARTEFACT those triangles are, named by a number no
   *  other artefact ever has. An executor that keeps something of its
   *  own per geometry — a device's uploaded buffers — keys on this and
   *  not on the address, because a dropped artefact frees its memory and
   *  the next one cooked can land on it. */
  uint64_t geometry = 0;
  glm::vec4 baseColor{0.8f, 0.8f, 0.85f, 1.0f};
  std::string_view key;
  /** The collections the body's node joined. */
  std::span<const std::string> collections;
  /** The keys from the root down to this body's parent. */
  std::span<const std::string> ancestors;
  const ::sigil::material::Material* material = nullptr;
  /** THE PER-FACE SLOTS of a body whose faces wear more than one: each
   *  triangle wears the slot its mesh's "Material" primitive lane names,
   *  and `material` is the first of them. Empty for a body wearing one
   *  surface. */
  std::span<const ::sigil::material::Material> slots;
  /** DO THE FRAME'S LIGHTS REACH THIS BODY? False for a surface that
   *  is its own light — a screen, a decal, an emissive set. It is read
   *  off the material once per frame, beside the map, so an executor
   *  never asks a material tree what kind of surface a body wears. */
  bool lit = true;
  /** THE MAP THE SURFACE IS DRESSED WITH: the base-colour texture the
   *  body's material carries, or null when it carries none. It is read
   *  off the material once per frame, so an executor does not walk a
   *  material tree per draw. */
  const ::sigil::material::Texture* texture = nullptr;
  /** WHETHER IT BLOCKS the lights that cast shadows — the node's word,
   *  before a light's shadow link and before what its surface is. */
  bool castShadow = true;
  /** Whether those lights' shadows darken it. */
  bool receiveShadow = true;
  /** WHETHER A TEXTURE PASS PAINTED, this frame, a page the surface
   *  wears: its pixels may differ from the frame before's while the
   *  surface, as a value, is the same. An executor keeping something it
   *  drew from the surface's pixels draws it again. */
  bool repainted = false;
};

/** A TEXTURE AS A MESH SAMPLES IT: where it is read at over the mesh's
 *  own uv coordinates, whether it repeats outside them and how it is
 *  read between its texels. The pixels are the executor's to bind.
 *  The matrix is carried across rather than copied — inverted and taken
 *  through the image's size — so a placement and a scale mean the same
 *  thing on a mesh as they do in a plane. */
struct Sampling {
  /** Where the image is read at, over the mesh's own uv coordinates, as
   *  a 2D affine matrix acting on column vectors `(u, v, 1)`. */
  glm::mat3 uv{1.0f};
  bool tile = false;
  /** How the image is read BETWEEN texels, carried across from the
   *  texture: nearest keeps a texel's edge hard, linear reads across
   *  it. */
  ::sigil::material::Sampling filter = ::sigil::material::Sampling::Linear;
};

/** @p texture as a mesh samples it, read off the texture and the size its
 *  source states, so no pixel crosses to answer it. An empty texture
 *  answers an empty Sampling. */
Sampling samplingOf(const ::sigil::material::Texture& texture);

/** WHERE A FRAME'S TARGETS STAND IN A LARGER PICTURE: one `whole` pixels
 *  across, of which the targets are the part whose top-left corner is at
 *  `origin`, counted down and across the picture as its rows run. The
 *  projection stays the whole picture's and is carried off-centre onto
 *  the part, so a pixel of the targets is the pixel the whole picture
 *  holds there and nothing outside the part is formed at all — which is
 *  how a host showing a magnified piece of a scene pays for the piece.
 *
 *  A zero `whole` says the targets ARE the picture, and so does one
 *  equal to the targets' size at the origin; either leaves every
 *  projection exactly the one a frame without an offset has. */
struct ViewOffset {
  glm::ivec2 whole{0, 0};
  glm::ivec2 origin{0, 0};

  /** Value equality: the picture's size and the part's corner. */
  bool operator==(const ViewOffset&) const = default;
};

/** Whether @p offset makes the targets, @p extent pixels across, a part
 *  of a larger picture rather than the picture itself. */
[[nodiscard]] bool isPart(const ViewOffset& offset, glm::ivec2 extent);

/** THE CLIP-SPACE CROP that carries the whole picture @p offset names
 *  onto targets @p extent pixels across: a scale and a shift of x and y
 *  applied after a projection, leaving depth where it was. The identity
 *  where the targets are the whole picture. */
[[nodiscard]] glm::mat4 cropOf(const ViewOffset& offset, glm::ivec2 extent);

/** WHAT ONE FRAME EXTRACTED, handed to every pass that runs over it.
 *  The bodies arrive sorted back to front by view depth, stably, so two
 *  at one depth stand in tree order — the order a blended body has to be
 *  drawn in over what stands behind it. */
struct View {
  std::span<const Draw> draws;
  /** Every light the frame gathered, in world space. Which bodies each
   *  reaches is its light link's to say. */
  std::span<const PlacedLight> lights;
  /** THE SET'S ENVIRONMENT, read once — the panorama every lit body
   *  samples for what reaches it from every direction, oriented by its
   *  turn. Invalid when the frame states none, and a lit body then falls
   *  back to the flat ambient it always had. */
  Environment environment;
  /** The lens the frame looks through, placed by its node. */
  geometry::mesh::camera::Camera camera;
  /** How the picture is read: what every radiance is multiplied by
   *  before the tone curve, and the curve — the camera's. */
  float exposure = 1.0f;
  ToneCurve toneCurve = ToneCurve::Reinhard;
  /** The size the frame's targets are made at. */
  glm::ivec2 extent{0, 0};
  /** THE FRAME'S TIME: the scene time the tree was described at, which a
   *  material reading the clock — a flipbook, a flow — is shown at. */
  std::chrono::duration<double> time{};
  /** Where the targets stand in the picture the camera frames; the
   *  targets are the whole of it unless this says otherwise. */
  ViewOffset offset;
  /** A body whose program is still compiling is drawn as a stand-in
   *  rather than waited for: `Frame::standIns`. */
  bool standIns = false;
  /** The device times each stage of the frame: `Frame::timeStages`. */
  bool timeStages = false;
  /** THE PICTURES LAID UNDER THE BODIES, in order: `Frame::backdrop`.
   *  Each is an authored picture, written over a painting pass's target
   *  after its clear and its sky as the light the final stage turns back
   *  into it. */
  std::span<const material::Texture> backdrops;
};

/** The size of the picture @p view's camera frames, in pixels: the
 *  offset's whole where the targets are a part of it, the targets'
 *  extent where they are all of it. The aspect every projection is
 *  formed at is this one's. */
[[nodiscard]] glm::ivec2 wholeOf(const View& view);

/** The camera's clip transform onto @p view's targets — the one
 *  `Camera::clipProjection` gives for the whole picture, cropped onto the
 *  part the targets are. A device draws with this. */
[[nodiscard]] glm::mat4 clipProjection(const View& view);

/** @p draw as a collection reads it. */
Member memberOf(const Draw& draw);

}  // namespace sigil::world
