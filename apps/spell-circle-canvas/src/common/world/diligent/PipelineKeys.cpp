/** @file
 * The pipelines bodies are drawn through: how one body's pipeline differs
 * from another's — into the picture, into a shadow map's depth, or into
 * the tint beside it — and the fixed ones every runtime builds ahead of
 * its first frame.
 */

#include <include/core/SkBlendMode.h>

#include "GeometryPass.h"

namespace sigil::world::diligent {

/** HOW A BODY'S PIPELINE DIFFERS from another built out of @p program:
 *  into the picture, by how it blends and whether it writes depth and
 *  drops its back faces; into a shadow map (@p caster), depth and no
 *  colour from both faces, or the tint laid over by multiplying. */
PipelineKey bodyKey(const material::slang::Compiled& program,
                    const Blending& blending, bool cull, const Caster* caster) {
  // A BODY WRITING DEPTH IS OPAQUE: what it covers it replaces, so a
  // body composited normally and writing depth is drawn with blending
  // off, which is also what lets the device skip the pixels a nearer
  // opaque body hides. Translucency is a body that writes no depth or
  // states a blend of its own.
  const SkBlendMode blend =
      blending.depthWrite && blending.blend == SkBlendMode::kSrcOver
          ? SkBlendMode::kSrc
          : blending.blend;
  PipelineKey key{&program, blend, true, blending.depthWrite, false, cull};
  key.target = kRadianceFormat;
  key.instanced = true;
  key.tangent = true;
  if (caster) {
    // Depth and no colour; both faces, so a card blocks the light from
    // behind as well as in front.
    key = PipelineKey{&program, SkBlendMode::kSrc, true, true, false, false};
    key.target = dg::TEX_FORMAT_UNKNOWN;
    key.instanced = true;
    key.tangent = true;
    if (caster->tint) {
      key.blend = SkBlendMode::kModulate;
      key.depthWrite = false;
      key.target = dg::TEX_FORMAT_RGBA8_UNORM;
    }
  }
  return key;
}

void prepareFixedPipelines(Gpu& gpu) {
  // THE BODIES' OWN: a stand-in and a body with no Slang body of its own,
  // opaque, with its back faces dropped and kept, and the two depth-only
  // programs a shadow map is drawn with.
  for (const bool cull : {true, false}) {
    gpu.pipelines.prepare(bodyKey(scaffold(false), Blending{}, cull, nullptr));
    gpu.pipelines.prepare(bodyKey(scaffold(true), Blending{}, cull, nullptr));
  }
  const Caster depthOnly{};
  gpu.pipelines.prepare(bodyKey(shadowScaffold(/*coverage=*/false), Blending{},
                                false, &depthOnly));
  gpu.pipelines.prepare(bodyKey(shadowScaffold(/*coverage=*/true), Blending{},
                                false, &depthOnly));
  // THE FRAME'S OWN: the sky, the clear, a copy and the final stage, each a
  // triangle over the whole target, the last into the display's format.
  const PostPrograms& programs = postPrograms();
  const auto stage = [&](const material::slang::Compiled& program,
                         dg::TEXTURE_FORMAT target) {
    PipelineKey key{&program, SkBlendMode::kSrc, false, false, true};
    key.target = target;
    gpu.pipelines.prepare(key);
  };
  stage(backdropProgram(), kRadianceFormat);
  stage(programs.fill, kRadianceFormat);
  stage(programs.copy, kRadianceFormat);
  stage(programs.final, kColorFormat);
}

}  // namespace sigil::world::diligent
