/** @file
 * Shaders as materials: the language from the option or the extension,
 * the frame inputs from the names the body spells, and one definition per
 * source, key, language, parameter layout and texture names, so a
 * re-described material prunes and an edited source compiles anew.
 */

#include "sigilmaterial/program/Shader.h"

#include <sigilio/hub/Hub.h>
#include <sigilmaterial/advanced/Program.h>  // reportOnce
#include <sigilmaterial/advanced/Recipe.h>
#include <sigilmaterial/texture/Texture.h>

#include <cstdint>
#include <cstdio>
#include <map>
#include <mutex>
#include <regex>
#include <string>
#include <tuple>

namespace sigil::material {

namespace {

struct Empty {};

bool spells(const std::string& text, std::string_view name) {
  const std::regex word("\\b" + std::string(name) + "\\b");
  return std::regex_search(text, word);
}

/** The name a shader with no key answers to: a hash of its text, which
 *  is stable from run to run, so a message names the same shader
 *  twice. */
std::string derivedName(std::string_view source, Target target) {
  uint64_t hash = 14695981039346656037ull;
  for (const char character : source) {
    hash ^= (unsigned char)character;
    hash *= 1099511628211ull;
  }
  char digits[17];
  std::snprintf(digits, sizeof digits, "%016llx", (unsigned long long)hash);
  return std::string("shader.") + std::string(name(target)) + "." + digits;
}

std::shared_ptr<const Recipe> define(std::string name, std::string_view source,
                                     Target target, const Schema& parameters,
                                     const ShaderOptions& options) {
  std::vector<std::string> textures;
  textures.reserve(options.textures.size());
  for (const ShaderTexture& texture : options.textures)
    textures.push_back(texture.name);
  // The layout is keyed by its fields rather than its address: a parameter
  // struct's layout lives in whichever binary instantiated it, and a
  // reloaded one may put a different struct at the same address.
  std::string layout;
  for (const Field& field : parameters.fields)
    layout += field.name + ":" + std::to_string((int)field.kind) + ":" +
              std::to_string(field.floats) + ";";
  using Key = std::tuple<std::string, std::string, Target, std::string,
                         std::vector<std::string>>;
  static std::mutex mutex;
  static std::map<Key, std::shared_ptr<const Recipe>> definitions;
  const std::lock_guard lock(mutex);
  Key key{std::move(name), std::string(source), target, std::move(layout),
          std::move(textures)};
  if (auto found = definitions.find(key); found != definitions.end())
    return found->second;
  const std::string& text = std::get<1>(key);
  Recipe recipe = Recipe::of(std::get<0>(key), parameters);
  for (const std::string& texture : std::get<4>(key)) recipe.slot(texture);
  // A frame input the parameter struct already declares is the author's
  // own uniform, and declaring it twice would not compile.
  for (FrameInput input : {FrameInput::Time, FrameInput::Resolution,
                           FrameInput::ContentScale,
                           FrameInput::WorldTransform})
    if (!parameters.find(uniformName(input)) &&
        spells(text, uniformName(input)))
      recipe.frame(input);
  recipe.body(target, text);
  auto made = std::make_shared<const Recipe>(std::move(recipe));
  definitions.emplace(std::move(key), made);
  return made;
}

}  // namespace

namespace detail {

std::shared_ptr<const Recipe> shaderDefinition(std::string_view source,
                                               const Schema& parameters,
                                               const ShaderOptions& options) {
  if (source.empty()) {
    reportOnce("shader-empty:" + options.key,
               "shader \"" + options.key +
                   "\" was given no source; it paints nothing");
    return nullptr;
  }
  const Target target = options.target.value_or(Target::SkSL);
  std::string name =
      options.key.empty() ? derivedName(source, target) : options.key;
  return define(std::move(name), source, target, parameters, options);
}

std::shared_ptr<const Recipe> shaderFileDefinition(
    io::Hub& hub, std::string_view uri, const Schema& parameters,
    const ShaderOptions& options) {
  std::optional<Target> target = options.target;
  if (!target && uri.ends_with(".sksl")) target = Target::SkSL;
  if (!target && uri.ends_with(".slang")) target = Target::Slang;
  if (!target) {
    reportOnce("shader-extension:" + std::string(uri),
               "shader \"" + std::string(uri) +
                   "\" is neither .sksl nor .slang and names no target; it "
                   "paints nothing");
    return nullptr;
  }
  const std::optional<std::string> text = hub.text(uri);
  if (!text) {
    reportOnce("shader-read:" + std::string(uri),
               "shader \"" + std::string(uri) +
                   "\" could not be read; it paints nothing");
    return nullptr;
  }
  std::string name = options.key.empty() ? std::string(uri) : options.key;
  return define(std::move(name), *text, *target, parameters, options);
}

Material withTextures(Material shader, const ShaderOptions& options) {
  const Sampling sampling = options.sampling.value_or(Sampling::Linear);
  for (const ShaderTexture& texture : options.textures)
    if (texture.pixels)
      shader.slot(texture.name, Texture(texture.pixels).sampling(sampling));
  return shader;
}

}  // namespace detail

Material shader(std::string_view source, const ShaderOptions& options) {
  return shader(source, Empty{}, options);
}

Material shader(io::Hub& hub, std::string_view uri,
                const ShaderOptions& options) {
  return shader(hub, uri, Empty{}, options);
}

}  // namespace sigil::material
