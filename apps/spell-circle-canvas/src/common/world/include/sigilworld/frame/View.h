#pragma once

/** @file
 * @ingroup world-frame
 * What a pass is allowed to see: the bodies a frame extracted, the
 * emitters, the viewpoint and the extent. It is read-only and it is the
 * ONLY door onto the scene an execution has — the description tree is
 * not reachable from a pass.
 */

#include <sigilgeometry/mesh/render/Painter.h>
#include <sigilgeometry/mesh/render/Shading.h>
#include <sigilmaterial/core/Backface.h>
#include <sigilmaterial/texture/Texture.h>
#include <sigilmedia/core/Picture.h>
#include <sigilworld/element/Element.h>
#include <sigilworld/element/Environment.h>
#include <sigilworld/element/Selector.h>

#include <glm/mat3x3.hpp>
#include <glm/mat4x4.hpp>
#include <glm/vec2.hpp>
#include <glm/vec4.hpp>
#include <span>
#include <string>
#include <string_view>

namespace sigil::world {

/** ONE BODY, as an execution reads it: where it stands, what triangles
 *  it is, what it is painted with, and the words a selector asks about.
 *  Every pointer and span here addresses state the frame retains, and
 *  stands for as long as the view does. */
struct Draw {
  glm::mat4 world{1.0f};
  const geometry::mesh::Mesh* mesh = nullptr;
  /** Whether reverse-wound faces survive rasterization. */
  material::Backface backface = material::Backface::Hidden;
  /** WHICH COOKED ARTEFACT those triangles are, named by a number no
   *  other artefact ever has. An executor that keeps something of its
   *  own per geometry — a device's uploaded buffers — keys on this and
   *  not on the address, because a dropped artefact frees its memory and
   *  the next one cooked can land on it. */
  uint64_t geometry = 0;
  glm::vec4 baseColor{0.8f, 0.8f, 0.85f, 1.0f};
  std::string_view key;
  std::span<const std::string> tags;
  /** The keys from the root down to this body's parent. */
  std::span<const std::string> ancestors;
  const ::sigil::material::Material* material = nullptr;
  /** DO THE FRAME'S EMITTERS REACH THIS BODY? False for a surface that
   *  is its own light — a screen, a decal, an emissive set. It is read
   *  off the material once per frame, beside the map, so an executor
   *  never asks a material tree what kind of surface a body wears. */
  bool lit = true;
  /** THE MAP THE SURFACE IS DRESSED WITH: the base-colour texture the
   *  body's material carries, or null when it carries none. It is read
   *  off the material once per frame, so an executor does not walk a
   *  material tree per draw. */
  const ::sigil::material::Texture* texture = nullptr;
};

/** A TEXTURE AS A MESH SAMPLES IT: the image, where it is read at over
 *  the mesh's own uv coordinates, and whether it repeats outside them.
 *  The matrix is carried across rather than copied — inverted and taken
 *  through the image's size — so a placement and a scale mean the same
 *  thing on a mesh as they do in a plane. */
struct Sampling {
  media::Picture image;
  /** Where the image is read at, over the mesh's own uv coordinates, as
   *  a 2D affine matrix acting on column vectors `(u, v, 1)`. */
  glm::mat3 uv{1.0f};
  bool tile = false;
  /** How the image is read BETWEEN texels, carried across from the
   *  texture: nearest keeps a texel's edge hard, linear reads across
   *  it. */
  ::sigil::material::Sampling filter = ::sigil::material::Sampling::Linear;
};

/** @p texture as a mesh samples it. An empty texture answers an empty
 *  Sampling, whose null image is a body that is simply not dressed. */
Sampling samplingOf(const ::sigil::material::Texture& texture);

/** WHAT A SURFACE IS BEYOND ITS COLOUR, as an executor reads it: how
 *  metallic, how rough, and the three glass terms. Read off the
 *  material's parameters by name, so a material built from some other recipe
 *  answers the values that leave the shading where it was. */
struct SurfaceTerms {
  float metallic = 0;
  float roughness = 0.5f;
  float transmission = 0;
  float ior = 1.5f;
  float thickness = 0;
  glm::vec3 absorption{0, 0, 0};
};
/** The shading terms @p material carries, or the ones that leave the
 *  shading where it was when it carries none. */
SurfaceTerms surfaceTermsOf(const ::sigil::material::Material* material);

/** AN EMITTER AS THE MESH PAINTER TAKES IT: the one directional reading
 *  every tier that shades without a per-pixel position works from. */
::sigil::geometry::mesh::render::Light painterLight(const material::Light& light);

/** The map @p body is dressed with and whether the emitters reach it,
 *  put on @p style — and taken off it again for a body carrying
 *  neither, since one style is reused across a whole list of them. */
void dress(::sigil::geometry::mesh::render::MeshStyle& style, const Draw& body);

/** THE ENVIRONMENT AS A MESH PAINTER TAKES IT: the prefiltered chain,
 *  the cosine convolution, and the orientation that carries a world
 *  direction into the panorama's frame. An invalid environment answers
 *  an empty one, which is a surface keeping the flat ambient it had. */
::sigil::geometry::mesh::render::Environment paintedEnvironment(
    const Environment& environment, const glm::mat3& orientation);

/** WHERE A FRAME'S TARGETS STAND IN A LARGER PICTURE: one `whole` pixels
 *  across, of which the targets are the part whose top-left corner is at
 *  `origin`, counted down and across the picture as its rows run. The
 *  projection stays the whole picture's and is carried off-centre onto
 *  the part, so a pixel of the targets is the pixel the whole picture
 *  holds there and nothing outside the part is formed at all — which is
 *  how a host showing a magnified piece of a set pays for the piece.
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
 *  at one depth stand in tree order — the order a rasteriser with no
 *  depth buffer must draw them in. */
struct View {
  std::span<const Draw> draws;
  std::span<const material::Light> lights;
  /** THE SET'S ENVIRONMENT MAP, oriented — the panorama every lit body
   *  samples for what reaches it from every direction. `orientation`
   *  carries a world-space direction into the panorama's own frame, so
   *  turning the node that placed it turns the sky. Invalid when the
   *  frame described none, and a lit body then falls back to the flat
   *  ambient it always had. */
  Environment environment;
  glm::mat3 orientation{1.0f};
  geometry::mesh::camera::Camera camera;
  /** The size the frame's targets are made at. */
  glm::ivec2 extent{0, 0};
  /** Where the targets stand in the picture the camera frames; the
   *  targets are the whole of it unless this says otherwise. */
  ViewOffset offset;
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

/** @p draw as a Selector reads it. */
Subject subjectOf(const Draw& draw);

}  // namespace sigil::world
