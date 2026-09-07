#pragma once

/** @file
 * The reader: a USD stage's meshes, point instancers and materials
 * poured into geometry::mesh::codec::decode::Model, the same currency every
 * other format lands in, and its emitters and cameras read back as the
 * values a scene is made of.
 */

#include <sigilgeometry/mesh/camera/Camera.h>
#include <sigilgeometry/mesh/codec/Model.h>
#include <sigilworld/element/Environment.h>
#include <sigilworld/light/Light.h>

#include <filesystem>
#include <optional>
#include <string>
#include <vector>

namespace sigil::usd {

/** The bound UsdPreviewSurface material names, in slot order, that
 *  readModel() found — parallel to Part::materialIndex. */
struct ReadInfo {
  std::vector<std::string> materialNames;
};

/** Read a USD file (.usd, .usda, .usdc, .usdz) into a Model: every
 *  UsdGeomMesh becomes a Part (xforms baked, faces triangulated, `st`
 *  as uvs, `displayColor` as colours, primvars as lanes, GeomSubsets as
 *  the "Material" lane with materialIndex), every point instancer's
 *  positions a faceless Part; a bound UsdPreviewSurface fills the
 *  Part's factors and texture references (bytes read from the file's
 *  neighbours). nullopt when the stage cannot be opened. */
std::optional<geometry::mesh::codec::decode::Model> readModel(
    const std::filesystem::path& file, std::string* error = nullptr);
/** The same read, additionally filling @p info with the material names
 *  the bindings carried — which the Model itself does not keep, because a
 *  Part refers to its material by index. */
std::optional<geometry::mesh::codec::decode::Model> readModel(
    const std::filesystem::path& file, ReadInfo* info,
    std::string* error = nullptr);

/** One emitter read from a stage, with the path of the prim it came
 *  from — the same string the Writer returned for it. */
struct ReadLight {
  std::string path;
  world::light::Light light;
};

/** ONE ENVIRONMENT MAP read from a stage: its dials, where the panorama
 *  is oriented, and the name of the file holding it. The panorama is NOT
 *  decoded — this library opens no image, the way it opens no texture
 *  for a material either — so `environment.map` is empty and `texture`
 *  is the path, relative to the stage, that a caller decodes and hands
 *  to `EnvironmentMap::fromEquirect`. */
struct ReadEnvironment {
  std::string path;
  std::string texture;
  world::Environment environment;
  glm::mat3 orientation{1.0f};
};

/** One camera read from a stage, with the path of the prim it came
 *  from. */
struct ReadCamera {
  std::string path;
  geometry::mesh::camera::Camera camera;
};

/** Every emitter on the stage, in traversal order: a UsdLuxDistantLight
 *  as a sun aimed along the prim's -Z, a UsdLuxSphereLight as a point
 *  light where the prim stands — or as a spot when the prim carries an
 *  authored shaping cone, whose angle is the outer half-angle and whose
 *  softness is how much of it the falloff eats. `sigil:range` gives the
 *  range; a light authored without it keeps the Light default. Other
 *  UsdLux shapes are skipped. nullopt when the stage cannot be
 *  opened. */
std::optional<std::vector<ReadLight>> readLights(
    const std::filesystem::path& file, std::string* error = nullptr);

/** Every UsdLuxDomeLight on the stage, in traversal order. The
 *  intensity read back carries the factor the writer divided the
 *  panorama by, so a stage written and read again lights a set at the
 *  radiance it was described with. nullopt when the stage cannot be
 *  opened. */
std::optional<std::vector<ReadEnvironment>> readEnvironments(
    const std::filesystem::path& file, std::string* error = nullptr);

/** Every UsdGeomCamera on the stage, in traversal order: the prim's
 *  local-to-world places the eye and aims it, the focal length against
 *  the vertical aperture gives the vertical field of view, and the
 *  clipping range the near and far planes. The target sits at the
 *  authored focus distance along the view direction, one unit ahead
 *  when the stage names none — the view is the same wherever on that
 *  ray it lands. nullopt when the stage cannot be opened. */
std::optional<std::vector<ReadCamera>> readCameras(
    const std::filesystem::path& file, std::string* error = nullptr);

}  // namespace sigil::usd
