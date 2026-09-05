/** @file
 * A body as a selector reads it, what its surface is beyond its colour,
 * and the environment as a mesh painter takes it.
 */

#include <sigilworld/frame/View.h>

#include <sigilmaterial/core/Recipe.h>

namespace sigil::world {

Sampling samplingOf(const material::Texture& texture) {
  Sampling out;
  out.image = texture.image();
  // A source may hold its pixels on a device and NOWHERE ELSE, in which
  // case there is no host image to read and the size comes from where
  // they stand — the placement is a question about the picture, not
  // about which side of the bus it is on.
  const material::DeviceImage where = texture.deviceImage();
  if (!out.image && !where) return {};
  // Either axis repeating is a repeat: a mesh's sampler has one wrap for
  // both, and clamping the axis that was asked to repeat would drag one
  // edge's pixels across the whole face.
  out.tile = texture.tileX() != SkTileMode::kClamp ||
             texture.tileY() != SkTileMode::kClamp;
  out.filter = texture.filter();

  const SkISize size = out.image ? out.image->dimensions()
                                 : SkISize::Make(where.width, where.height);
  SkMatrix lookup;
  if (size.isEmpty() || !texture.uv().invert(&lookup)) return out;
  out.uv =
      SkMatrix::Scale(1.0f / (float)size.width(), 1.0f / (float)size.height());
  out.uv.preConcat(lookup);
  out.uv.preConcat(SkMatrix::Scale((float)size.width(), (float)size.height()));
  return out;
}

Subject subjectOf(const Draw& draw) {
  return Subject{draw.key, draw.tags, draw.ancestors, draw.material};
}


SurfaceTerms surfaceTermsOf(const ::sigil::material::Material* material) {
  SurfaceTerms terms;
  if (!material) return terms;
  const material::Schema& params = material->recipe().params();
  const auto scalar = [&](std::string_view name, float& into) {
    const material::Field* field = params.find(name);
    if (field && field->kind == material::Kind::Float)
      into = material->get<float>(name);
  };
  scalar("metallic", terms.metallic);
  scalar("roughness", terms.roughness);
  scalar("transmission", terms.transmission);
  scalar("ior", terms.ior);
  scalar("thickness", terms.thickness);
  const material::Field* absorb = params.find("absorption");
  if (absorb && absorb->kind == material::Kind::Color) {
    const glm::vec4 value = material->get<glm::vec4>("absorption");
    terms.absorption = {value.r, value.g, value.b};
  }
  return terms;
}

::sigil::geometry::mesh::render::Environment paintedEnvironment(
    const Environment& environment, const glm::mat3& orientation) {
  namespace render = ::sigil::geometry::mesh::render;
  render::Environment out;
  // THE EXPOSURE CROSSES FIRST, before the panorama is asked for: it is
  // the one dial that means something in a set carrying no map at all,
  // because a lit sum ends at the tone curve whether or not there is a
  // sky over it.
  out.exposure = environment.exposure;
  if (!environment.valid()) return out;
  // The chain and the convolution are baked once per panorama and kept
  // by the value, so asking for them every frame is a lookup.
  out.levels = environment.map.chain();
  out.irradiance = environment.map.irradiance();
  if (environment.next.valid()) {
    out.nextLevels = environment.next.chain();
    out.nextIrradiance = environment.next.irradiance();
  }
  out.crossfade = environment.crossfade;
  out.orientation = orientation;
  out.tint = environment.tint;
  out.intensity = environment.intensity;
  out.diffuse = environment.diffuse;
  out.specular = environment.specular;
  out.roughnessBias = environment.roughnessBias;
  out.backdrop = environment.backdrop.intensity;
  out.backdropBlur = environment.backdrop.blur;
  out.groundRadius = environment.backdrop.groundRadius;
  out.projectionCenter = environment.backdrop.projectionCenter;
  return out;
}

}  // namespace sigil::world
