/** @file
 * A body as a selector reads it, what its surface is beyond its colour,
 * and the environment as a mesh painter takes it.
 */

#include <sigilmaterial/advanced/Recipe.h>
#include <sigilmaterial/skia/Paint.h>
#include <sigilmaterial/skia/Texture.h>
#include <sigilworld/advanced/Skia.h>
#include <sigilworld/frame/View.h>
#include <sigilworld/light/Light.h>

namespace sigil::world {

Sampling samplingOf(const material::Texture& texture) {
  Sampling out;
  out.image = material::skia::image(texture);
  // A source may hold its pixels on a device and NOWHERE ELSE, in which
  // case there is no host image to read and the size comes from where
  // they stand — the placement is a question about the picture, not
  // about which side of the bus it is on.
  const material::DeviceImage where = texture.deviceImage();
  if (!out.image && !where) return {};
  // Either axis repeating is a repeat: a mesh's sampler has one wrap for
  // both, and clamping the axis that was asked to repeat would drag one
  // edge's pixels across the whole face.
  out.tile = texture.tileX() != material::Repeat::Pad ||
             texture.tileY() != material::Repeat::Pad;
  out.filter = texture.sampling();

  const SkISize size = out.image ? media::toSk(out.image.size())
                                 : SkISize::Make(where.width, where.height);
  SkMatrix lookup;
  if (size.isEmpty() || !material::skia::toSkMatrix(texture.uv()).invert(&lookup))
    return out;
  SkMatrix uv =
      SkMatrix::Scale(1.0f / (float)size.width(), 1.0f / (float)size.height());
  uv.preConcat(lookup);
  uv.preConcat(SkMatrix::Scale((float)size.width(), (float)size.height()));
  out.uv = fromSk(uv);
  return out;
}

Subject subjectOf(const Draw& draw) {
  return Subject{draw.key, draw.tags, draw.ancestors, draw.material};
}

::sigil::geometry::mesh::render::Light painterLight(const material::Light& light) {
  const light::Directional value = light::directional(light);
  ::sigil::geometry::mesh::render::Light out;
  out.direction = value.direction;
  out.color = SkColor4f{value.color.r, value.color.g, value.color.b, 1.0f};
  out.intensity = value.intensity;
  return out;
}

void dress(::sigil::geometry::mesh::render::MeshStyle& style,
           const Draw& body) {
  const Sampling sampling =
      body.texture ? samplingOf(*body.texture) : Sampling{};
  style.texture = media::toSk(sampling.image);
  style.uvTransform = toSk(sampling.uv);
  style.tileTexture = sampling.tile;
  style.filter = toSk(sampling.filter);
  style.lit = body.lit;
  style.backfaceCull = body.backface == material::Backface::Hidden;
  const SurfaceTerms terms = surfaceTermsOf(body.material);
  style.metallic = terms.metallic;
  style.roughness = terms.roughness;
}

SurfaceTerms surfaceTermsOf(const ::sigil::material::Material* material) {
  SurfaceTerms terms;
  if (!material) return terms;
  const material::Schema& parameters = material->recipe().parameters();
  const auto scalar = [&](std::string_view name, float& into) {
    const material::Field* field = parameters.find(name);
    if (field && field->kind == material::ParameterType::Float)
      into = material->get<float>(name);
  };
  scalar("metallic", terms.metallic);
  scalar("roughness", terms.roughness);
  scalar("transmission", terms.transmission);
  scalar("ior", terms.ior);
  scalar("thickness", terms.thickness);
  const material::Field* absorb = parameters.find("absorption");
  if (absorb && absorb->kind == material::ParameterType::Color) {
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
  const auto images = [](const std::vector<material::Texture>& levels) {
    std::vector<sk_sp<SkImage>> out;
    out.reserve(levels.size());
    for (const material::Texture& level : levels)
      out.push_back(material::skia::image(level));
    return out;
  };
  out.levels = images(environment.map.chain());
  out.irradiance = material::skia::image(environment.map.irradiance());
  if (environment.next.valid()) {
    out.nextLevels = images(environment.next.chain());
    out.nextIrradiance = material::skia::image(environment.next.irradiance());
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
