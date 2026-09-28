#pragma once

/** @file
 * @ingroup material-core
 *
 * A SHADER AS A MATERIAL: `shader(source, Parameters{…})` is a material
 * whose base is the body @p source over the uniforms a parameter struct
 * declares. The body alone is written — for SkSL `half4 main(float2 p)
 * { … }` and its helpers, for Slang `float4 surface(float2 uv) { … }` —
 * and the declarations are generated: one uniform per field of the
 * struct, one sampled texture per name in `ShaderOptions::textures`, and
 * `uTime`, `uResolution`, `uContentScale` and `uWorld` wherever the body
 * spells them. The answer is a Material like any other: a base, a
 * `layer()` source, and a field written with `set(name, value)` or
 * followed with `bind(name, animatable)`.
 */

#include <sigilmaterial/core/Material.h>
#include <sigilmaterial/core/Parameters.h>
#include <sigilmaterial/core/Target.h>
#include <sigilmedia/core/PixelSource.h>

#include <concepts>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace sigil::io {
class Hub;
}  // namespace sigil::io

namespace sigil::material {

/** How a texture is read between its pixels, declared with its values in
 *  `<sigilmaterial/texture/Texture.h>`. */
enum class Sampling : uint8_t;

/** A TEXTURE A SHADER SAMPLES, by the name its body reads it as. Empty
 *  pixels declare the name and leave it to be filled later with
 *  `Material::slot`. */
struct ShaderTexture {
  std::string name;
  media::PixelSource pixels;
  bool operator==(const ShaderTexture&) const = default;
};

/** HOW A SHADER IS DEFINED beyond its source and its parameters. */
struct ShaderOptions {
  /** The name the shader answers to in messages; empty derives one from
   *  the source. Two shaders are one definition when their source, key,
   *  language, parameter struct and texture names all agree. */
  std::string key;
  /** The language of the source; unset reads a text as SkSL and a file
   *  by its extension (`.sksl`, `.slang`). A Slang body is drawn by a
   *  renderer that compiles Slang. */
  std::optional<Target> target;
  /** How every texture is read between its pixels; unset reads them
   *  linearly. */
  std::optional<Sampling> sampling;
  /** The textures the body samples, each declared by its name. */
  std::vector<ShaderTexture> textures;
  bool operator==(const ShaderOptions&) const = default;
};

namespace detail {
/** The definition `shader()` instances: one per distinct source, key,
 *  language, parameter layout and texture names, held for the life of the
 *  process, so describing the same shader again answers the same
 *  definition and a node over it prunes. Null when @p source is empty. */
std::shared_ptr<const Recipe> shaderDefinition(std::string_view source,
                                               const Schema& parameters,
                                               const ShaderOptions& options);
/** The definition of the program file at @p uri, read through @p hub:
 *  its newest text that compiled, or its newest text while no compiler
 *  for its language is registered to judge it. Null when no text of it
 *  has compiled, it cannot be read, or its language cannot be told. What
 *  is wrong stands on the hub's problems under @p uri until a text
 *  compiles. */
std::shared_ptr<const Recipe> shaderFileDefinition(
    io::Hub& hub, std::string_view uri, const Schema& parameters,
    const ShaderOptions& options);
/** @p shader with every texture @p options names placed where its body
 *  samples it. */
Material withTextures(Material shader, const ShaderOptions& options);
}  // namespace detail

/** THE SHADER @p source AS A MATERIAL over @p parameters, whose fields are
 *  the body's uniforms by name. An empty source is a material of
 *  nothing. */
template <class Parameters>
  requires(!std::same_as<Parameters, ShaderOptions>)
Material shader(std::string_view source, const Parameters& parameters,
                const ShaderOptions& options = {}) {
  std::shared_ptr<const Recipe> definition =
      detail::shaderDefinition(source, schema<Parameters>(), options);
  if (!definition) return Color{0, 0, 0, 0};
  return detail::withTextures(Material(std::move(definition), parameters),
                              options);
}

/** THE SHADER @p source with no parameters of its own. */
Material shader(std::string_view source, const ShaderOptions& options = {});

/** THE MATERIAL A PROGRAM FILE PAINTS WHILE NONE OF ITS TEXTS HAS
 *  COMPILED: a magenta and black checker, sixteen pixels a cell. It is a
 *  diagnostic — it says a program is missing where the program would
 *  paint — and never a look. */
Material placeholder();

/** THE PROGRAM FILE at @p uri, read through @p hub, as a material over
 *  @p parameters, which is how a file is live-coded. Each call reads the
 *  file as the hub holds it, so an edited file compiles anew once the
 *  hub's poll has seen the edit. A text that does not compile never
 *  replaces one that did: the newest text that compiled keeps painting.
 *  While none has — the file is missing, its first text is broken, its
 *  extension names no language — `placeholder()` paints. Whatever is
 *  wrong is reported on the hub's `problems()` under @p uri, with the
 *  compiler's message and the body's line when it names one, and taken
 *  back when a text compiles. A text is judged once a compiler for its
 *  language is registered; before that it is drawn as it stands. */
template <class Parameters>
  requires(!std::same_as<Parameters, ShaderOptions>)
Material shader(io::Hub& hub, std::string_view uri,
                const Parameters& parameters,
                const ShaderOptions& options = {}) {
  std::shared_ptr<const Recipe> definition = detail::shaderFileDefinition(
      hub, uri, schema<Parameters>(), options);
  if (!definition) return placeholder();
  return detail::withTextures(Material(std::move(definition), parameters),
                              options);
}

/** THE PROGRAM FILE at @p uri with no parameters of its own. */
Material shader(io::Hub& hub, std::string_view uri,
                const ShaderOptions& options = {});

}  // namespace sigil::material
