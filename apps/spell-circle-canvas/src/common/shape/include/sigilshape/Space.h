#pragma once

/** @file
 * SigilShape space — Skia's 3D capabilities put to work. Two devices:
 *
 *  - drawPanel(): a full SkM44 (perspective included) concat'd onto the
 *    canvas, then ordinary 2D drawing — Skia rasterizes perspective-
 *    correct, which is exactly what a diegetic UI card needs. Panels
 *    are the zero-copy path: any SkImage (a compose scene, a web view,
 *    an SVG) lands on a plane in space.
 *
 *  - drawMesh(): the painter pipeline for Mesh solids — CPU transform,
 *    per-vertex lighting, back-to-front triangle sort, SkVertices
 *    batches (chunked under the 16-bit index limit). MeshStyle::Mode::Normals
 *    renders a device-space normal G-buffer — +y down, matching the
 *    canvas — instead of lit color; feed that surface to Materials.h
 *    and per-pixel chrome/gold/glass lands on true 3D geometry — the
 *    deferred bridge between the two headers.
 *
 * One Camera struct drives both renderers. 3D data (camera vectors,
 * model/view/projection matrices) speaks glm; this header is the
 * BRIDGE where it meets Skia's canvas — toSkM44() is the seam.
 */

#include <include/core/SkCanvas.h>
#include <include/core/SkColor.h>
#include <include/core/SkImage.h>
#include <include/core/SkM44.h>
#include <include/core/SkRefCnt.h>

#include <glm/glm.hpp>
#include <vector>

#include "sigilshape/Mesh.h"

namespace sigil::shape::space {

/** The glm -> Skia seam: both are column-major, so the conversion is a
 *  straight pour. */
inline SkM44 toSkM44(const glm::mat4& m) { return SkM44::ColMajor(&m[0][0]); }

/** Right-handed, y-up camera. Field of view is vertical. */
struct Camera {
  glm::vec3 eye = {0, 0, 480};
  glm::vec3 target = {0, 0, 0};
  glm::vec3 up = {0, 1, 0};
  float fovYDeg = 40;
  float zNear = 4;
  float zFar = 4096;

  glm::mat4 view() const;
  glm::mat4 projection(float aspect) const;
  /** view -> NDC -> viewport pixels (y flipped back to Skia's y-down). */
  glm::mat4 viewProjection(SkSize viewport) const;
};

struct Light {
  glm::vec3 direction = {-0.5f, -0.8f, -0.4f};  ///< world-space, toward scene
  SkColor4f color = SkColors::kWhite;
  float intensity = 1;
};

struct MeshStyle {
  enum class Mode : uint8_t {
    Lit,      ///< per-vertex Lambert + Blinn specular + rim
    Normals,  ///< device-space normal G-buffer, +y down — the Materials.h
              ///< convention (rgb = (n.x, -n.y, n.z)*0.5+0.5), unlit
    Uv,       ///< uv debug ramp
  };
  Mode mode = Mode::Lit;
  SkColor4f baseColor = {0.8f, 0.8f, 0.85f, 1};
  std::vector<Light> lights = {{}};
  SkColor4f ambient = {0.12f, 0.12f, 0.15f, 1};
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
  /** PRIMITIVE lane (Mesh::prims) multiplied into each triangle's
   *  colour — flat per-face tint, no vertex duplication. Empty = off;
   *  a missing or mis-sized lane is ignored. Lit mode only: Normals
   *  and Uv render BUFFERS, and a tint there would corrupt them. */
  std::string primColorLane;
  bool backfaceCull = true;
  bool depthSort = true;
};

/** Draw a mesh through the painter pipeline. @p model is the mesh's
 *  world transform; the camera provides view/projection at the
 *  canvas's @p viewport size. */
void drawMesh(SkCanvas& canvas, const Mesh& mesh, const glm::mat4& model,
              const Camera& camera, SkSize viewport,
              const MeshStyle& style = {});

/** Place 2D content on a plane in space: concats the full perspective
 *  transform then runs @p draw with the canvas in the panel's local
 *  coordinates (origin at panel center, x right, y DOWN like any Skia
 *  canvas, one unit = one world unit). */
void drawPanel(SkCanvas& canvas, const glm::mat4& model, const Camera& camera,
               SkSize viewport, const std::function<void(SkCanvas&)>& draw);

/** Convenience: an image mapped onto a width x height panel at
 *  @p model (image stretched to the panel rect, centered). */
void drawImagePanel(SkCanvas& canvas, sk_sp<SkImage> image, float width,
                    float height, const glm::mat4& model, const Camera& camera,
                    SkSize viewport, float opacity = 1);

/** Model-matrix helpers (row-major reading order: applied right to
 *  left, translate * rotate * scale). */
glm::mat4 place(glm::vec3 position, float yawDeg = 0, float pitchDeg = 0,
                float rollDeg = 0, float scale = 1);

/** The BILLBOARD transform: content placed at @p at with its +z face —
 *  mesh::quad()'s facing convention — pointed at @p eye. Camera math
 *  both renderers want: a billboard is the same panel re-described each
 *  frame with a fresh faceCamera(), which a reconciler sees as a
 *  transform-only change (setTransform, never a re-upload). The basis
 *  is detail::basisFor — the SAME construction points::instance() and
 *  world's instanced path stamp with, so a faceCamera'd quad and a
 *  facing-lane instance orient identically; Dir≈±up falls back the same
 *  way, and eye==at degenerates to facing +z. */
glm::mat4 faceCamera(glm::vec3 eye, glm::vec3 at, glm::vec3 up = {0, 1, 0});

}  // namespace sigil::shape::space
