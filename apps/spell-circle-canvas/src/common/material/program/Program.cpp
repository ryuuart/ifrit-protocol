/** @file
 * Program files as recipes: the language from the extension, the frame
 * inputs from the names the body spells, and one recipe per file text and
 * parameter layout, so a re-described material prunes and an edited file
 * compiles anew.
 */

#include "sigilmaterial/program/Program.h"

#include <sigilmaterial/core/Program.h>  // reportOnce

#include <map>
#include <mutex>
#include <regex>
#include <tuple>

namespace sigil::material {

namespace {

struct Empty {};

bool spells(const std::string& text, std::string_view name) {
  const std::regex word("\\b" + std::string(name) + "\\b");
  return std::regex_search(text, word);
}

}  // namespace

std::shared_ptr<const Recipe> programRecipe(io::Hub& hub, std::string_view uri,
                                            const Schema& parameters,
                                            const ProgramOptions& options) {
  Target target;
  if (uri.ends_with(".sksl")) {
    target = Target::SkSL;
  } else if (uri.ends_with(".slang")) {
    target = Target::Slang;
  } else {
    reportOnce("program-extension:" + std::string(uri),
               "program \"" + std::string(uri) +
                   "\" is neither .sksl nor .slang; it paints nothing");
    return nullptr;
  }
  const std::optional<std::string> text = hub.text(uri);
  if (!text) {
    reportOnce("program-read:" + std::string(uri),
               "program \"" + std::string(uri) +
                   "\" could not be read; it paints nothing");
    return nullptr;
  }
  using Key = std::tuple<std::string, std::string, const Schema*,
                         std::vector<std::string>>;
  static std::mutex mutex;
  static std::map<Key, std::shared_ptr<const Recipe>> recipes;
  const std::lock_guard lock(mutex);
  Key key{std::string(uri), *text, &parameters, options.slots};
  if (auto found = recipes.find(key); found != recipes.end())
    return found->second;
  Recipe recipe = Recipe::of(std::string(uri), parameters);
  for (const std::string& slot : options.slots) recipe.slot(slot);
  for (FrameInput input : {FrameInput::Time, FrameInput::Resolution,
                           FrameInput::ContentScale,
                           FrameInput::WorldTransform})
    if (spells(*text, uniformName(input))) recipe.frame(input);
  recipe.body(target, *text);
  auto made = std::make_shared<const Recipe>(std::move(recipe));
  recipes.emplace(std::move(key), made);
  return made;
}

Material program(io::Hub& hub, std::string_view uri,
                 const ProgramOptions& options) {
  return program(hub, uri, Empty{}, options);
}

}  // namespace sigil::material
