#pragma once

/** @file
 * @ingroup material-skia
 *
 * A LIT SURFACE IN 2D through Skia: a material whose `surface()` states a
 * response, shaded under a `Lighting` — its colour stack prepared once,
 * as it would be painted flat, and a lighting pass over it that reads the
 * normal map for relief. A bound light re-resolves the pass while its
 * inputs retain their sources and resolve against the destination.
 */

#include <include/core/SkRefCnt.h>
#include <include/core/SkShader.h>
#include <sigilmaterial/advanced/FrameData.h>
#include <sigilmaterial/core/Lighting.h>
#include <sigilmaterial/core/Material.h>
#include <sigilmaterial/paint/Paint.h>

#include <memory>

namespace sigil::material::skia {

/** Whether @p material states a surface that takes light: one stated and
 *  not `unlit`. */
bool isLit(const Material& material);

/** Whether the colour stack, surface maps, effects, positioned sources or a
 *  stated scene lighting frame depend on root coordinates. A cached consumer
 * must repaint when its node-to-root placement changes. */
bool usesWorldSpace(const Material& material);

/** The lighting @p material is shaded under where @p inForce is the
 *  scene's: its own surface's `lighting` over the scene's. Empty where
 *  neither states one, which paints the material flat. */
Lighting lightingFor(const Material& material, const Lighting& inForce);

/** A material's prepared colour stack, surface maps and lighting setup.
 *  Retain one while the material stays the same. Copies share immutable
 *  inputs and the most recent lighting setup; live bindings and children
 *  resolve for each draw. Preparation retains sources without reading
 *  pixels. Resolution binds them to the draw's destination.
 *  The current environment's lowered image is retained independently, so
 *  changes to its rotation, strength or size reuse that source. */
class LitSurface {
 public:
  explicit LitSurface(const Material& material);
  /** An ordinary paint with its frameless snapshot, including source reads. */
  Paint under(const Lighting& lighting) const;
  /** Resolve one draw whose input unit square maps into the node through
   *  @p paintToLocal. Color and channel inputs share the prepared lowering;
   *  their frame has unit resolution and placement through that mapping.
   *  Encoded normals retain their slopes; height-derived normals receive
   *  logical-pixel sampling steps through the mapping. Lighting and the
   *  result use node coordinates. The environment resolves in the node frame.
   *  Nonfinite or noninvertible mappings draw nothing. Projective mappings
   *  keep colors and encoded normals but flatten height-derived relief.
   */
  sk_sp<SkShader> shader(const Lighting& lighting, const FrameData& nodeFrame,
                         const glm::mat3& paintToLocal) const;

 private:
  struct Inputs;
  struct PassCache;
  std::shared_ptr<const Inputs> m_inputs;
  std::shared_ptr<PassCache> m_pass;
  Paint lightingPass(const Lighting& lighting) const;
  Paint makeLightingPass(const Lighting& lighting) const;
};

/** @p material's colour stack SHADED UNDER @p lighting, as one paint: live
 *  while an angle, a strength or the environment moves, static
 *  otherwise. With no lighting, or a material that is not lit, the paint
 *  is the colour stack alone, as `paint(material)` lowers it.
 *  Point/spot positions are root-page logical pixels over the flat Z=0 page;
 *  normals cross affine XY transforms by inverse transpose, with Z unchanged.
 *  Perspective and singular/near-singular XY transforms suppress direct
 *  point/spot illumination. Ambient, environment and emission remain.
 *  At zero source distance the direction toward the viewer is used.
 *  Direct contributions and ambient shares add; an environment alone uses
 *  a full ambient share. Reflection, emission and coating attenuation run
 *  once. Surface-frame directional sources use local normals; its environment
 *  uses page normals when any source is positioned. Scene-frame lighting
 *  uses page normals for every source and the environment. */
Paint lit(const Material& material, const Lighting& lighting);

}  // namespace sigil::material::skia
