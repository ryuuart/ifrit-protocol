/** @file
 * The sky: which of a program's slots are the frame's panorama and the
 * split sum's table, the table itself as a texture, and the backdrop —
 * the panorama drawn over the whole target before every body in it.
 */

#include <include/core/SkBitmap.h>
#include <sigilmaterial/advanced/Terms.h>
#include <sigilmaterial/texture/Texture.h>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <glm/gtc/matrix_inverse.hpp>
#include <string>
#include <string_view>
#include <vector>

#include "GeometryPass.h"

namespace sigil::world::diligent {

bool isEnvironmentSlot(std::string_view slot) {
  for (std::string_view name : kEnvironmentSlots)
    if (slot == name) return true;
  return slot == kBrdfSlot;
}

/** THE SPLIT SUM'S TABLE as a texture: SigilMaterial's integral of the
 *  lobe, scale in red and bias in green, and beside them in blue the
 *  sheen's directional albedo over the same square — so a sheen costs no
 *  table of its own — made once for the process. Eight bits a channel hold
 *  it to well under what the shading can show. */
const material::Texture& brdfTable() {
  static const material::Texture table = [] {
    const int size = material::kEnvironmentBrdfSize;
    const std::vector<glm::vec2>& values = material::environmentBrdfTable();
    const std::vector<float>& sheen = material::sheenAlbedoTable();
    SkBitmap bitmap;
    bitmap.allocPixels(SkImageInfo::Make(size, size, kRGBA_8888_SkColorType,
                                         kPremul_SkAlphaType));
    for (int y = 0; y < size; ++y)
      for (int x = 0; x < size; ++x) {
        const glm::vec2 ab = values[(size_t)y * size + x];
        uint8_t* texel = (uint8_t*)bitmap.getAddr32(x, y);
        texel[0] = (uint8_t)std::lround(std::clamp(ab.x, 0.0f, 1.0f) * 255.0f);
        texel[1] = (uint8_t)std::lround(std::clamp(ab.y, 0.0f, 1.0f) * 255.0f);
        texel[2] = (uint8_t)std::lround(
            std::clamp(sheen[(size_t)y * size + x], 0.0f, 1.0f) * 255.0f);
        texel[3] = 255;
      }
    bitmap.setImmutable();
    return material::Texture(bitmap.asImage())
        .tile(material::Repeat::Pad)
        .sampling(material::Sampling::Linear);
  }();
  return table;
}

/** THE SKY, drawn over the whole target before every body in it. It is a
 *  pass and not a body because a body would need a mesh, a placement and
 *  a depth, and a sky has none of the three: it is what is there when
 *  nothing else is. */
void drawBackdrop(Gpu& gpu, const View& view, const glm::mat4& projection,
                  const glm::mat4& viewMatrix) {
  const Environment& sky = view.environment;
  if (!sky.valid() || sky.shown <= 0) return;
  const material::slang::Compiled& program = backdropProgram();
  if (program.empty()) return;
  dg::ITexture* panorama =
      prefilteredPanorama(gpu, gpu.maps.environment(sky.map));
  if (!panorama) return;
  dg::ITexture* panoramaNext =
      prefilteredPanorama(gpu, gpu.maps.environment(sky.next));
  if (!panoramaNext) panoramaNext = panorama;
  dg::ITexture* lobe = gpu.maps.irradiance(sky.map);
  dg::ITexture* lobeNext = gpu.maps.irradiance(sky.next);
  if (!lobeNext) lobeNext = lobe;

  // The sky is opaque and stands behind everything, so it writes no
  // depth and takes none: whatever is drawn after it covers it.
  PipelineKey key{&program, SkBlendMode::kSrc, false, false, true};
  key.target = kRadianceFormat;
  const Pipeline* pipeline = gpu.pipelines.pipeline(key);
  if (!pipeline) return;

  material::slang::Uniforms uniforms(program);
  uniforms.set("uEnvTint", sky.intensity, sky.intensity, sky.intensity,
               std::clamp(sky.crossfade, 0.0f, 1.0f));
  uniforms.set("uEnvDials", sky.diffuse, sky.specular, sky.roughnessBias,
               (float)panorama->GetDesc().MipLevels);
  uniforms.set("uTone", gpu.exposure, curveNumber(gpu.toneCurve), 0.0f, 0.0f);
  uniforms.set("uBackdrop", sky.shown, std::clamp(sky.blur, 0.0f, 1.0f), 0.0f,
               0.0f);
  // The ground sphere stands at the world origin, carried into the space
  // the ray is found in, where the eye is the origin — so the eye's own
  // position is never stated twice.
  const glm::vec3 centre = glm::vec3(viewMatrix * glm::vec4(0, 0, 0, 1.0f));
  uniforms.set("uGround", centre.x, centre.y, centre.z, sky.groundRadius);
  uniforms.set("uEnvMatrix",
               glm::mat4(sky.orientation *
                         glm::transpose(glm::mat3(
                             glm::inverseTranspose(glm::mat3(viewMatrix))))));
  uniforms.set("uInvProj", glm::inverse(projection));

  std::vector<dg::ITexture*> textures(program.textures.size(), nullptr);
  for (size_t i = 0; i < program.textures.size(); ++i) {
    const std::string& slot = program.textures[i];
    if (slot == kEnvironmentSlots[0])
      textures[i] = panorama;
    else if (slot == kEnvironmentSlots[1])
      textures[i] = panoramaNext;
    else if (slot == kEnvironmentSlots[2])
      textures[i] = lobe;
    else if (slot == kEnvironmentSlots[3])
      textures[i] = lobeNext;
  }

  dg::IDeviceContext* context = gpu.device->context();
  context->SetPipelineState(pipeline->state);
  device::bindDraw(gpu.shared, *pipeline, program, uniforms, textures,
                   SkFilterMode::kLinear, false, &isEnvironmentSlot);
  dg::DrawAttribs draw;
  draw.NumVertices = 3;
  draw.Flags = dg::DRAW_FLAG_VERIFY_ALL;
  context->Draw(draw);
}

}  // namespace sigil::world::diligent
