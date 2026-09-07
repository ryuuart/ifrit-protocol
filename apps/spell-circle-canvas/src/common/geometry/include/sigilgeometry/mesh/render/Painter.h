#pragma once

/** @file
 * Drawing meshes and panels. Two devices:
 *
 *  - drawPanel(): a full perspective transform concat'd onto the
 *    canvas, then ordinary 2D drawing — the rasterizer works
 *    perspective-correct, which is exactly what a diegetic UI card
 *    needs. Panels are the zero-copy path: any SkImage (a compose
 *    scene, a web view, an SVG) lands on a plane in space.
 *
 *  - drawMesh(): transform, per-vertex lighting, back-to-front triangle
 *    sort and emission for Mesh solids. MeshStyle::Mode::Normals
 *    renders a device-space normal G-buffer — +y down, matching the
 *    canvas — instead of lit colour; feed that surface to a surface
 *    recipe and per-pixel chrome, gold or glass lands on true 3D
 *    geometry.
 *
 * Both run on the Runtime the style carries, defaulting to the built-in
 * CPU one, so the vocabulary here — Mesh, glm vectors and matrices, the
 * camera, MeshStyle — is the same whichever executor performs the work.
 */

#include <include/core/SkCanvas.h>
#include <include/core/SkColor.h>
#include <include/core/SkImage.h>
#include <include/core/SkMatrix.h>
#include <include/core/SkRefCnt.h>
#include <include/core/SkSamplingOptions.h>
#include <include/core/SkSize.h>

#include <functional>
#include <glm/glm.hpp>
#include <string>
#include <vector>

#include "sigilgeometry/mesh/Mesh.h"
#include "sigilgeometry/mesh/camera/Camera.h"
#include "sigilgeometry/mesh/render/Runtime.h"
#include "sigilgeometry/mesh/render/Shading.h"

namespace sigil::geometry::mesh::render {

/** One directional light. `direction` is the direction the light
 *  TRAVELS in world space, so it points from the lamp toward the
 *  scene rather than back at it. */
struct Light {
  glm::vec3 direction = {-0.5f, -0.8f, -0.4f};  ///< world-space, toward scene
  SkColor4f color = SkColors::kWhite;
  float intensity = 1;
};

/** Everything the mesh shader needs beyond the geometry itself: which
 *  of the three shading modes to run, the surface's base colour and
 *  optional texture, the lights the Lit mode answers to, and the
 *  runtime the draw executes on. */
struct MeshStyle {
  enum class Mode : uint8_t {
    Lit,      ///< per-vertex Lambert + Blinn specular + rim
    Normals,  ///< device-space normal G-buffer, +y down — the convention
              ///< surface recipes read (rgb = (n.x, -n.y, n.z)*0.5+0.5), unlit
    Uv,       ///< uv debug ramp
  };
  Mode mode = Mode::Lit;
  /** Do the lights reach this surface? False is a surface that is its
   *  own light — a screen, a decal, an emissive set: what it shows is
   *  its base colour and the mesh's own tint, with no ambient, no
   *  emitter, no specular and no rim. It is a property of the SURFACE,
   *  which is why it sits beside the colour rather than among the modes:
   *  Normals and Uv render buffers, and this still renders a picture. */
  bool lit = true;
  SkColor4f baseColor = {0.8f, 0.8f, 0.85f, 1};
  std::vector<Light> lights = {{}};
  SkColor4f ambient = {0.12f, 0.12f, 0.15f, 1};
  /** THE PANORAMA THE SURFACE SEES PAST THE LIGHTS. Carrying one
   *  replaces the flat `ambient` above with what actually falls on the
   *  surface from each direction, and gives it something to mirror; an
   *  empty one leaves the constant standing, which is what keeps a set
   *  described without a sky the picture it already was. */
  Environment environment;
  /** How much of a metal the surface is, and how rough — the two the
   *  environment terms need that a Blinn highlight had no use for. Both
   *  are one number over the whole surface here: this tier has no map
   *  and no per-pixel half. */
  float metallic = 0;
  float roughness = 0.5f;
  float specular = 0.5f;  ///< Blinn specular strength
  float shininess = 48;   ///< Blinn exponent
  float rim = 0.25f;      ///< rim light strength
  /** Optional texture: uvs sample this image, modulated by lighting. */
  sk_sp<SkImage> texture;
  /** Texture PLACEMENT in uv space, applied before the lookup —
   *  translate to scroll (a marquee riding a ribbon), scale to repeat,
   *  rotate to spin. Identity = the image spans uv [0,1] once. */
  SkMatrix uvTransform = SkMatrix::I();
  /** Wrap the texture when uvs leave [0,1] (a scrolling band on a
   *  closed loop); off = clamp, the panel default. */
  bool tileTexture = false;
  /** How the texture is read BETWEEN texels. Nearest keeps a texel's
   *  edge hard — a pixel-art map, an index map, an atlas whose cells
   *  must not bleed — and takes no mip level with it, because blending
   *  two levels is the same bleed arriving by the other door; linear
   *  reads between texels and between levels. */
  SkFilterMode filter = SkFilterMode::kLinear;
  /** PRIMITIVE lane (Mesh::prims) multiplied into each triangle's
   *  colour — flat per-face tint, no vertex duplication. Empty = off;
   *  a missing or mis-sized lane is ignored. Lit mode only: Normals
   *  and Uv render BUFFERS, and a tint there would corrupt them. */
  std::string primColorLane;
  bool backfaceCull = true;
  bool depthSort = true;
  /** Who performs the draw. The default is the built-in CPU executor;
   *  assigning another one is the whole of switching runtimes. */
  Runtime runtime = Runtime::cpu();
};

/** Draw a mesh. @p model is the mesh's world transform; the camera
 *  provides view/projection at the canvas's @p viewport size; the
 *  style's runtime performs the work. */
void drawMesh(SkCanvas& canvas, const Mesh& mesh, const glm::mat4& model,
              const camera::Camera& camera, SkSize viewport,
              const MeshStyle& style = {});

/** Place 2D content on a plane in space: concats the full perspective
 *  transform then runs @p draw with the canvas in the panel's local
 *  coordinates (origin at panel center, x right, y DOWN like any Skia
 *  canvas, one unit = one world unit). */
void drawPanel(SkCanvas& canvas, const glm::mat4& model,
               const camera::Camera& camera, SkSize viewport,
               const std::function<void(SkCanvas&)>& draw,
               const Runtime& runtime = Runtime::cpu());

/** Convenience: an image mapped onto a width x height panel at
 *  @p model (image stretched to the panel rect, centered). */
void drawImagePanel(SkCanvas& canvas, sk_sp<SkImage> image, float width,
                    float height, const glm::mat4& model,
                    const camera::Camera& camera, SkSize viewport,
                    float opacity = 1, const Runtime& runtime = Runtime::cpu());

}  // namespace sigil::geometry::mesh::render
