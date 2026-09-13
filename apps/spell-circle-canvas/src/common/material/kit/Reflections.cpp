/** @file
 * The gold, chrome and glass bodies in SkSL — the shared prelude of
 * normal decode, equirectangular lookup and value noise, then each reflection
 * model — and the builders that fill their texture slots.
 */

#include "sigilmaterial/kit/Reflections.h"

#include <sigilshaders/MaterialKit.h>

#include <algorithm>
#include <string>
#include <string_view>

#include "NoisePrelude.h"

namespace sigil::material::kit {

namespace {

glm::vec2 sizeOf(const EnvironmentMap& env) {
  const SkISize s = env.size();
  return {(float)std::max(s.width(), 1), (float)std::max(s.height(), 1)};
}

}  // namespace

const std::shared_ptr<const Recipe>& goldRecipe() {
  static const auto recipe = std::make_shared<const Recipe>(
      Recipe::of<GoldParameters>("gold").slot("normals").slot("env").body(
          Target::SkSL, std::string(noisePrelude(Target::SkSL))
                            .append(shaderSource("ReflectivePrelude.sksl"))
                            .append(shaderSource("ReflectiveGold.sksl"))));
  return recipe;
}

const std::shared_ptr<const Recipe>& chromeRecipe() {
  static const auto recipe = std::make_shared<const Recipe>(
      Recipe::of<ChromeParameters>("chrome").slot("normals").slot("env").body(
          Target::SkSL, std::string(noisePrelude(Target::SkSL))
                            .append(shaderSource("ReflectivePrelude.sksl"))
                            .append(shaderSource("ReflectiveChrome.sksl"))));
  return recipe;
}

const std::shared_ptr<const Recipe>& glassRecipe() {
  static const auto recipe = std::make_shared<const Recipe>(
      Recipe::of<GlassParameters>("glass")
          .slot("normals")
          .slot("env")
          .slot("backdrop")
          .body(Target::SkSL,
                std::string(noisePrelude(Target::SkSL))
                    .append(shaderSource("ReflectivePrelude.sksl"))
                    .append(shaderSource("ReflectiveGlass.sksl"))));
  return recipe;
}

Material gold(Texture normals, const EnvironmentMap& env,
              const GoldParameters& parameters) {
  GoldParameters p = parameters;
  p.envSize = sizeOf(env);
  Material m(goldRecipe(), p);
  m.slot("normals", std::move(normals));
  m.slot("env", env.texture(parameters.roughness));
  return m;
}

Material chrome(Texture normals, const EnvironmentMap& env,
                const ChromeParameters& parameters) {
  ChromeParameters p = parameters;
  p.envSize = sizeOf(env);
  Material m(chromeRecipe(), p);
  m.slot("normals", std::move(normals));
  m.slot("env", env.texture(parameters.roughness));
  return m;
}

Material glass(Texture normals, const EnvironmentMap& env, Texture backdrop,
               const GlassParameters& parameters) {
  GlassParameters p = parameters;
  p.envSize = sizeOf(env);
  Material m(glassRecipe(), p);
  m.slot("normals", std::move(normals));
  m.slot("env", env.texture(parameters.roughness));
  m.slot("backdrop", std::move(backdrop));
  return m;
}

}  // namespace sigil::material::kit
