/** @file
 * A decoded part's factors lowered onto SigilMaterial's surface options.
 */

#include "sigilgeometry/mesh/codec/Surface.h"

namespace sigil::geometry::mesh::codec::decode {

namespace {

material::Color colour(glm::vec3 value) {
  return {value.r, value.g, value.b, 1};
}

}  // namespace

material::SurfaceOptions surfaceOptions(const Part& part) {
  material::SurfaceOptions out;
  out.metallic = part.metallic;
  out.roughness = part.roughness;
  if (part.emissive.r > 0 || part.emissive.g > 0 || part.emissive.b > 0) {
    out.emission = {part.emissive.r, part.emissive.g, part.emissive.b, 1};
    out.emissionStrength = 1;
  }
  out.alphaCutoff = part.alphaCutoff;
  out.transmission = part.transmission;
  out.ior = part.ior;
  out.thickness = part.thickness;
  out.attenuationColor = colour(part.attenuationColor);
  out.attenuationDistance = part.attenuationDistance;
  out.dispersion = part.dispersion;
  out.sheenColor = colour(part.sheenColor);
  out.sheenRoughness = part.sheenRoughness;
  out.anisotropy = part.anisotropy;
  out.anisotropyRotation = part.anisotropyRotation;
  out.iridescence = part.iridescence;
  out.iridescenceIor = part.iridescenceIor;
  out.iridescenceThickness = part.iridescenceThickness;
  out.iridescenceThicknessMinimum = part.iridescenceThicknessMinimum;
  out.specular = part.specular;
  out.specularColor = colour(part.specularColor);
  out.clearcoat = part.clearcoat;
  out.clearcoatRoughness = part.clearcoatRoughness;
  return out;
}

}  // namespace sigil::geometry::mesh::codec::decode
