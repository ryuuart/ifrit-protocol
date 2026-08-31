#pragma once

/** @file
 * What a pass is allowed to see: the bodies a frame extracted, the
 * emitters, the viewpoint and the extent. It is read-only and it is the
 * ONLY door onto the scene an execution has — the description tree is
 * not reachable from a pass.
 */

#include <include/core/SkImage.h>
#include <include/core/SkMatrix.h>
#include <include/core/SkRefCnt.h>
#include <include/core/SkSamplingOptions.h>
#include <include/core/SkSize.h>
#include <sigilmaterial/texture/Texture.h>
#include <sigilworld/element/Element.h>
#include <sigilworld/element/Selector.h>

#include <glm/mat4x4.hpp>
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
  const Mesh* mesh = nullptr;
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
 *  the mesh's own uv coordinates, and whether it repeats outside
 *  them.
 *
 *  A mesh carries normalised uvs, and a `material::Texture` states its
 *  placement in the image's own pixels, so the matrix is carried across
 *  rather than copied: it is inverted (a texture's matrix puts the image
 *  INTO the sampled space, and a lookup goes the other way) and taken
 *  through the image's size, so `at()` and a scale mean the same thing
 *  and point the same way on a mesh as they do in a plane. */
struct Sampling {
  sk_sp<SkImage> image;
  SkMatrix uv = SkMatrix::I();
  bool tile = false;
  /** How the image is read BETWEEN texels, carried across from the
   *  texture: nearest keeps a texel's edge hard, linear reads across
   *  it. */
  SkFilterMode filter = SkFilterMode::kLinear;
};

/** @p texture as a mesh samples it. An empty texture answers an empty
 *  Sampling, whose null image is a body that is simply not dressed. */
Sampling samplingOf(const ::sigil::material::Texture& texture);

/** WHAT ONE FRAME EXTRACTED, handed to every pass that runs over it.
 *
 *  The bodies arrive sorted back to front by view depth — stably, so
 *  two at one depth stand in tree order — because that is the order a
 *  rasteriser with no depth buffer must draw them in, and sorting once
 *  per frame rather than once per pass is what keeps two passes over
 *  one view drawing the same picture. */
struct View {
  std::span<const Draw> draws;
  std::span<const Light> lights;
  Camera camera;
  SkISize extent{0, 0};
};

/** @p draw as a Selector reads it. */
Subject subjectOf(const Draw& draw);

}  // namespace sigil::world
