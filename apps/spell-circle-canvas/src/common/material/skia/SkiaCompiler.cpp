/** @file
 * SkSL compilation through SkRuntimeEffect::MakeForShader, uniform upload
 * from layout bytes, and the recursive child binding that makes one
 * shader from a material tree — a material child resolves and binds its
 * own shader, a shader leaf binds the shader it yields.
 *
 * The compile also refuses a body that would compile here and not on a
 * device, which is one named thing: a declaration of a name the GPU
 * backend reserves for a parameter of its own.
 */

#include "sigilmaterial/skia/SkiaCompiler.h"

#include <include/core/SkString.h>
#include <sigilmaterial/texture/ShaderLeaf.h>

#include <cctype>
#include <cstring>
#include <mutex>
#include <string>
#include <string_view>
#include <utility>

namespace sigil::material::skia {

void SkiaProgram::upload(SkRuntimeShaderBuilder& builder,
                         std::span<const std::byte> bytes) const {
  for (const Field& f : recipe().layout().fields) {
    // The effect declares every field the layout holds, because its source
    // was generated from the same layout; a name Skia does not find means
    // the SkSL optimiser dropped an unused uniform, which is a silent skip
    // and not an error.
    SkRuntimeShaderBuilder::BuilderUniform u = builder.uniform(f.name);
    if (!u.fVar) continue;
    if (f.offset + f.floats * sizeof(float) > bytes.size()) continue;
    u.set(reinterpret_cast<const float*>(bytes.data() + f.offset),
          (int)f.floats);
  }
}

namespace {

/** @p source with the text of every comment, line or block, replaced by
 *  spaces, so a word scan reads code and not prose. Offsets are kept. */
std::string uncommented(std::string_view source) {
  std::string out(source);
  for (size_t i = 0; i + 1 < out.size();) {
    if (out[i] == '/' && out[i + 1] == '/') {
      while (i < out.size() && out[i] != '\n') out[i++] = ' ';
    } else if (out[i] == '/' && out[i + 1] == '*') {
      out[i++] = ' ';
      out[i++] = ' ';
      while (i + 1 < out.size() && !(out[i] == '*' && out[i + 1] == '/'))
        out[i++] = ' ';
      if (i + 1 < out.size()) {
        out[i++] = ' ';
        out[i++] = ' ';
      }
    } else {
      ++i;
    }
  }
  return out;
}

bool wordChar(char c) {
  return c == '_' || (c >= '0' && c <= '9') || (c >= 'a' && c <= 'z') ||
         (c >= 'A' && c <= 'Z');
}

/** Is @p word one of the type names a declaration can start with? The
 *  set is what a material body actually declares things as; a type it
 *  misses lets a collision through, which is why the compile-time proof
 *  of the rule is the device test and this is the early word. */
bool typeName(std::string_view word) {
  static constexpr std::string_view kScalars[] = {"float", "half", "int",
                                                  "uint",  "bool", "short"};
  for (std::string_view scalar : kScalars) {
    if (word == scalar) return true;
    if (word.size() > scalar.size() && word.starts_with(scalar)) {
      // The vectors and matrices: float2, half4, float3x3.
      const std::string_view tail = word.substr(scalar.size());
      if (tail.size() == 1 && tail[0] >= '2' && tail[0] <= '4') return true;
      if (tail.size() == 3 && tail[0] >= '2' && tail[0] <= '4' &&
          tail[1] == 'x' && tail[2] >= '2' && tail[2] <= '4')
        return true;
    }
  }
  return false;
}

/** The half-open span of `main`'s parameter list in @p code, or an empty
 *  span when there is none: the one place a reserved name may be
 *  declared, since that declaration is the one Graphite replaces. */
std::pair<size_t, size_t> mainParameters(const std::string& code) {
  for (size_t at = code.find("main"); at != std::string::npos;
       at = code.find("main", at + 4)) {
    if (at > 0 && wordChar(code[at - 1])) continue;
    size_t open = at + 4;
    while (open < code.size() && std::isspace((unsigned char)code[open]))
      ++open;
    if (open >= code.size() || code[open] != '(') continue;
    const size_t close = code.find(')', open);
    if (close == std::string::npos) continue;
    return {open, close};
  }
  return {0, 0};
}

// workaround: Graphite does not compile a runtime effect as its own
// program. It inlines the body into the pipeline's fragment shader as a
// helper whose parameters it names itself — `pos` for the coordinates,
// `inColor` for the colour arriving from the stage before, `destColor`
// for a blender's destination, `primitiveColor` for the draw's own — and
// it discards the names `main` declared, rewriting references to them as
// those. A body declaring anything else by one of those names therefore
// redeclares a parameter, which SkRuntimeEffect::MakeForShader cannot
// see (there the body IS the whole program and the name is free) and the
// device's compiler rejects, every frame, silently. So the names are
// refused here, where the recipe is compiled at all: the four words are
// Skia's, and a body wanting one of them renames its own.
std::string reservedName(const std::string& source) {
  static constexpr std::string_view kReserved[] = {"pos", "inColor",
                                                   "destColor",
                                                   "primitiveColor"};
  const std::string code = uncommented(source);
  const auto [paramsFrom, paramsTo] = mainParameters(code);
  for (std::string_view name : kReserved) {
    for (size_t at = code.find(name); at != std::string::npos;
         at = code.find(name, at + 1)) {
      if (at > 0 && wordChar(code[at - 1])) continue;
      const size_t end = at + name.size();
      if (end < code.size() && wordChar(code[end])) continue;
      if (at > paramsFrom && at < paramsTo) continue;  // main's own
      // A declaration is a type name immediately before it.
      size_t word = at;
      while (word > 0 && std::isspace((unsigned char)code[word - 1])) --word;
      const size_t wordEnd = word;
      while (word > 0 && wordChar(code[word - 1])) --word;
      if (wordEnd > word && typeName(std::string_view(code).substr(
                                word, wordEnd - word)))
        return std::string(name);
    }
  }
  return {};
}

std::shared_ptr<Program> compile(std::shared_ptr<const Recipe> recipe,
                                 Variant variant, std::string& error) {
  const std::string source = recipe->source(Target::SkSL);
  if (const std::string reserved = reservedName(source); !reserved.empty()) {
    error = "the body declares `" + reserved +
            "`, which is the name Graphite gives one of the parameters it "
            "inlines the body under, so the shader compiles here and not on "
            "the device: rename it";
    return nullptr;
  }
  auto [effect, message] =
      SkRuntimeEffect::MakeForShader(SkString(source.c_str()));
  if (!effect) {
    error = message.c_str();
    return nullptr;
  }
  return std::make_shared<SkiaProgram>(std::move(recipe), variant,
                                       std::move(effect));
}

}  // namespace

void install() {
  static std::once_flag once;
  std::call_once(once, [] { registerCompiler(Target::SkSL, compile); });
}

std::unique_ptr<SkRuntimeShaderBuilder> builder(
    const Material& material, const FrameData& frame, Variant variant,
    std::span<const std::string_view> leave) {
  const Material::Resolved resolved =
      material.resolve(Target::SkSL, frame, variant);
  if (!resolved.program) return nullptr;
  const auto* program = resolved.program->as<SkiaProgram>();
  if (!program) return nullptr;
  auto b = std::make_unique<SkRuntimeShaderBuilder>(program->effect());
  program->upload(*b, resolved.bytes);
  for (const auto& [slot, child] : material.children()) {
    bool skip = false;
    for (std::string_view name : leave) skip |= name == slot;
    if (skip) continue;
    SkRuntimeShaderBuilder::BuilderChild c = b->child(slot);
    if (!c.fChild) continue;
    if (child.material) {
      c = shader(*child.material, frame, variant);
    } else if (const auto* leaf =
                   dynamic_cast<const ShaderLeaf*>(child.leaf.get())) {
      c = leaf->shader();
    }
  }
  return b;
}

sk_sp<SkShader> shader(const Material& material, const FrameData& frame,
                       Variant variant) {
  std::unique_ptr<SkRuntimeShaderBuilder> b = builder(material, frame, variant);
  return b ? b->makeShader() : nullptr;
}

}  // namespace sigil::material::skia
