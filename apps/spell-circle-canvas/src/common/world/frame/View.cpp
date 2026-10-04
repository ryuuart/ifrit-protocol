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

#include <algorithm>

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
  if (size.isEmpty() ||
      !material::skia::toSkMatrix(texture.uv()).invert(&lookup))
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

::sigil::geometry::mesh::render::Light painterLight(
    const material::Light& light) {
  const light::Directional value = light::directional(light);
  ::sigil::geometry::mesh::render::Light out;
  out.direction = value.direction;
  out.color = glm::vec4{value.color.r, value.color.g, value.color.b, 1.0f};
  out.intensity = value.intensity;
  return out;
}

void dress(::sigil::geometry::mesh::render::MeshStyle& style,
           const Draw& body) {
  const Sampling sampling =
      body.texture ? samplingOf(*body.texture) : Sampling{};
  style.texture = sampling.image;
  style.uvTransform = ::sigil::geometry::path::Transform{sampling.uv};
  style.tileTexture = sampling.tile;
  style.filter = sampling.filter == material::Sampling::Nearest
                     ? ::sigil::geometry::mesh::render::Sampling::Nearest
                     : ::sigil::geometry::mesh::render::Sampling::Linear;
  style.lit = body.lit;
  style.backfaceCull = body.backface == material::Backface::Hidden;
  const SurfaceTerms terms = surfaceTermsOf(body.material);
  style.metallic = terms.metallic;
  style.roughness = terms.roughness;
  // The light a surface gives off of its own is laid over the shading,
  // where neither the emitters nor the base-colour map reach it.
  style.emissionMap = body.emissive && body.lit
                          ? samplingOf(*body.emissive).image
                          : media::Picture{};
  style.emission = terms.emission;
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
  const material::Field* emissive = parameters.find("emissive");
  if (emissive && emissive->kind == material::ParameterType::Color) {
    float strength = 0;
    scalar("emissiveStrength", strength);
    const glm::vec4 value = material->get<glm::vec4>("emissive");
    terms.emission =
        glm::vec3{value.r, value.g, value.b} * std::max(strength, 0.0f);
  }
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
    std::vector<media::Picture> out;
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

bool isPart(const ViewOffset& offset, glm::ivec2 extent) {
  if (offset.whole.x <= 0 || offset.whole.y <= 0) return false;
  if (extent.x <= 0 || extent.y <= 0) return false;
  return offset.whole != extent || offset.origin != glm::ivec2{0, 0};
}

glm::mat4 cropOf(const ViewOffset& offset, glm::ivec2 extent) {
  if (!isPart(offset, extent)) return glm::mat4(1.0f);
  // Clip x runs from -1 at the picture's left column to +1 at its right,
  // and clip y from +1 at its top row to -1 at its bottom — the rows run
  // down the picture and y up the clip space. The part's own edges are
  // put at those same values, so the part's columns and rows are the
  // whole picture's columns and rows from `origin` on.
  const glm::vec2 whole{(float)offset.whole.x, (float)offset.whole.y};
  const glm::vec2 part{(float)extent.x, (float)extent.y};
  const glm::vec2 origin{(float)offset.origin.x, (float)offset.origin.y};
  glm::mat4 crop(1.0f);
  crop[0][0] = whole.x / part.x;
  crop[1][1] = whole.y / part.y;
  // A shift in clip space is a multiple of w, which is what keeps it
  // exact through the perspective divide.
  crop[3][0] = (whole.x - 2.0f * origin.x - part.x) / part.x;
  crop[3][1] = (part.y - whole.y + 2.0f * origin.y) / part.y;
  return crop;
}

glm::ivec2 wholeOf(const View& view) {
  return isPart(view.offset, view.extent) ? view.offset.whole : view.extent;
}

glm::mat4 clipProjection(const View& view) {
  const glm::mat4 whole = view.camera.clipProjection(wholeOf(view));
  if (!isPart(view.offset, view.extent)) return whole;
  return cropOf(view.offset, view.extent) * whole;
}

}  // namespace sigil::world
