#pragma once

/** @file
 * @ingroup material-program
 *
 * A PROGRAM FILE AS A MATERIAL: an `.sksl` or `.slang` recipe body read
 * through the resource hub, instanced over a parameter struct. The file
 * holds the body alone — for SkSL `half4 main(float2 xy) { … }` and its
 * helpers, for Slang `float4 surface(float2 uv) { … }` — and the
 * declarations are generated: one uniform per field of the parameter
 * struct, one `uniform shader` per declared slot, and `uTime`,
 * `uResolution`, `uContentScale` and `uWorld` wherever the body spells
 * them.
 */

#include <sigilio/hub/Hub.h>
#include <sigilmaterial/core/Material.h>
#include <sigilmaterial/core/Parameters.h>
#include <sigilmaterial/core/Recipe.h>

#include <concepts>
#include <memory>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace sigil::material {

/** What a program file declares beyond its parameters. */
struct ProgramOptions {
  /** The slots the body samples by name, each filled later with
   *  `Material::slot`. */
  std::vector<std::string> slots;
};

/** THE RECIPE a program file defines over @p parameters, read through
 *  @p hub. Defined once per file text, so describing the same file again
 *  answers the same recipe and a material over it prunes; an edited file
 *  answers a new one. Null when the file cannot be read or its extension
 *  names no language (said once). */
std::shared_ptr<const Recipe> programRecipe(io::Hub& hub, std::string_view uri,
                                            const Schema& parameters,
                                            const ProgramOptions& options = {});

/** THE PROGRAM at @p uri as a material with @p parameters' values. An
 *  unreadable file is a material of nothing. */
template <class Parameters>
  requires(!std::same_as<Parameters, ProgramOptions>)
Material program(io::Hub& hub, std::string_view uri,
                 const Parameters& parameters,
                 const ProgramOptions& options = {}) {
  std::shared_ptr<const Recipe> recipe =
      programRecipe(hub, uri, schema<Parameters>(), options);
  if (!recipe) return Material();
  return Material(std::move(recipe), parameters);
}

/** THE PROGRAM at @p uri with no parameters of its own. */
Material program(io::Hub& hub, std::string_view uri,
                 const ProgramOptions& options = {});

}  // namespace sigil::material
