/** @file
 * Shaders as materials: the language from the option or the extension,
 * the frame inputs from the names the body spells, and one definition per
 * source, key, language, parameter layout and texture names, so a
 * re-described material prunes and an edited source compiles anew. A
 * file is judged as it is read: the last text that compiled keeps
 * painting while a later one does not, the placeholder paints while none
 * has, and what the compiler said stands on the hub's problems until a
 * text compiles.
 */

#include "sigilmaterial/program/Shader.h"

#include <sigilio/advanced/Places.h>
#include <sigilio/advanced/Problems.h>
#include <sigilio/hub/Hub.h>
#include <sigilmaterial/advanced/Program.h>  // reportOnce
#include <sigilmaterial/advanced/Recipe.h>
#include <sigilmaterial/texture/Texture.h>

#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <filesystem>
#include <map>
#include <mutex>
#include <optional>
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

std::vector<std::string> textureNames(const ShaderOptions& options) {
  std::vector<std::string> textures;
  textures.reserve(options.textures.size());
  for (const ShaderTexture& texture : options.textures)
    textures.push_back(texture.name);
  return textures;
}

/** The layout is keyed by its fields rather than its address: a
 *  parameter struct's layout lives in whichever binary instantiated it,
 *  and a reloaded one may put a different struct at the same address. */
std::string layoutOf(const Schema& parameters) {
  std::string layout;
  for (const Field& field : parameters.fields)
    layout += field.name + ":" + std::to_string((int)field.kind) + ":" +
              std::to_string(field.floats) + ";";
  return layout;
}

std::shared_ptr<const Recipe> define(std::string name, std::string_view source,
                                     Target target, const Schema& parameters,
                                     const ShaderOptions& options) {
  using Key = std::tuple<std::string, std::string, Target, std::string,
                         std::vector<std::string>>;
  static std::mutex mutex;
  static std::map<Key, std::shared_ptr<const Recipe>> definitions;
  const std::lock_guard lock(mutex);
  Key key{std::move(name), std::string(source), target, layoutOf(parameters),
          textureNames(options)};
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

/** The line of the body a compiler's @p message is about, counted from
 *  the body's first line. Skia's `error: LINE:` and a Slang-style
 *  `(LINE)` or `(LINE, COLUMN):` both count the lines of the whole
 *  program, so the declarations generated before the body are taken
 *  off. Unset when the message names no line, or one before the body. */
std::optional<int> bodyLine(const std::string& message,
                            const std::string& program,
                            const std::string& body) {
  static const std::regex skia(R"(error: (\d+):)");
  static const std::regex slang(R"(\((\d+)(?:,\s*\d+)?\)\s*:)");
  std::smatch match;
  if (!std::regex_search(message, match, skia) &&
      !std::regex_search(message, match, slang))
    return std::nullopt;
  const int line = std::stoi(match[1].str());
  const size_t at = program.find(body);
  const int before =
      at == std::string::npos
          ? 0
          : (int)std::count(program.begin(),
                            program.begin() + (std::ptrdiff_t)at, '\n');
  if (line - before < 1) return std::nullopt;
  return line - before;
}

/** One program file as `shader(hub, uri, …)` has read it for one
 *  definition key: the text last read and its definition, whether that
 *  definition compiled, the newest definition that did, and what stands
 *  on the hub about it. */
struct ReadFile {
  enum class Verdict { Unjudged, Compiled, Failed };
  std::string text;
  std::shared_ptr<const Recipe> current;
  Verdict verdict = Verdict::Unjudged;
  std::shared_ptr<const Recipe> compiled;
  std::optional<io::Problem> problem;
};

/** The magenta and black checker, sixteen pixels a cell in SkSL and
 *  eight cells across the surface in Slang, declaring nothing a caller
 *  could fill. */
std::shared_ptr<const Recipe> placeholderDefinition() {
  static const std::shared_ptr<const Recipe> definition = [] {
    Recipe recipe = Recipe::of("material.placeholder", Schema{});
    recipe.body(Target::SkSL, R"(
half4 main(float2 p) {
  float2 cell = floor(p / 16.0);
  half on = half(mod(cell.x + cell.y, 2.0));
  return half4(on, 0.0, on, 1.0);
}
)");
    recipe.body(Target::Slang, R"(
float4 surface(float2 uv) {
  float2 cell = floor(uv * 8.0);
  float on = fmod(cell.x + cell.y, 2.0);
  return float4(on, 0.0, on, 1.0);
}
)");
    return std::make_shared<const Recipe>(std::move(recipe));
  }();
  return definition;
}

}  // namespace

Material placeholder() { return Material(placeholderDefinition()); }

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
    const std::string message =
        "is neither .sksl nor .slang and names no target";
    io::reportProblem(hub, {std::string(uri), message, {}});
    reportOnce("shader-extension:" + std::string(uri),
               "shader \"" + std::string(uri) + "\" " + message +
                   "; the placeholder paints");
    return nullptr;
  }
  std::string name = options.key.empty() ? std::string(uri) : options.key;
  // One record per place a file stands and definition key: the file a
  // URI resolves to tells two hubs' `res://a.sksl` apart, and a URI no
  // mount answers stands for itself.
  const std::filesystem::path place = io::resolve(hub, uri);
  using Key = std::tuple<std::string, std::string, Target, std::string,
                         std::vector<std::string>>;
  static std::mutex mutex;
  static std::map<Key, ReadFile> files;
  const std::optional<std::string> text = hub.text(uri);
  const std::lock_guard lock(mutex);
  ReadFile& file =
      files[Key{place.empty() ? std::string(uri) : place.string(), name,
                *target, layoutOf(parameters), textureNames(options)}];
  // A text is judged once a compiler for its language is registered;
  // until then it is drawn as it stands, and a compile that fails at the
  // draw says so once on the diagnostic stream. A text drawn unjudged is
  // judged before a newer one replaces it, so the program it was is kept
  // if the newer one does not compile.
  const auto judge = [&] {
    if (!file.current || file.verdict != ReadFile::Verdict::Unjudged ||
        !ProgramCache::shared().hasCompiler(*target))
      return;
    std::string error;
    if (ProgramCache::shared().program(file.current, *target, {}, error)) {
      file.verdict = ReadFile::Verdict::Compiled;
      file.compiled = file.current;
      file.problem.reset();
    } else {
      file.verdict = ReadFile::Verdict::Failed;
      file.problem = io::Problem{
          std::string(uri), error,
          bodyLine(error, file.current->source(*target), file.text)};
    }
  };
  if (!text) {
    judge();
    file.problem = io::Problem{std::string(uri), "could not be read", {}};
  } else {
    if (!file.current || *text != file.text) {
      judge();
      file.text = *text;
      file.current = define(name, *text, *target, parameters, options);
      file.verdict = ReadFile::Verdict::Unjudged;
    }
    judge();
    if (file.verdict != ReadFile::Verdict::Failed) file.problem.reset();
  }
  // Put on the hub on every read while it stands, so a host that
  // cleared the list before its program asked again hears it again.
  if (file.problem) {
    io::reportProblem(hub, *file.problem);
    // A compile that failed was said by the program cache already.
    if (file.verdict != ReadFile::Verdict::Failed || !text)
      reportOnce("shader-problem:" + std::string(uri) + ":" +
                     file.problem->message,
                 "shader \"" + std::string(uri) + "\": " +
                     file.problem->message +
                     (file.compiled ? "; the last program that compiled paints"
                                    : "; the placeholder paints"));
  } else {
    io::clearProblem(hub, uri);
  }
  if (text && file.verdict == ReadFile::Verdict::Unjudged) return file.current;
  return file.compiled;
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
