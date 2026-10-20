/** @file
 * The three stacking bodies in SkSL, the composition that writes the
 * same three readings in Slang out of the operands' own bodies, and the
 * builder that fills the slots either way.
 */

#include "sigilmaterial/advanced/Combine.h"

#include <sigilmaterial/advanced/Program.h>
#include <sigilshaders/MaterialCore.h>

#include <boost/container/flat_map.hpp>
#include <mutex>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "Identifiers.h"

namespace sigil::material {

namespace {

/** THE THREE STACKING BODIES: which of the three a blend mode stacks
 *  through. */
enum class Blend : uint8_t { Mix, Add, Multiply };

/** The body @p mode stacks through; a mode with no stacking body is
 *  reported once and stacks as a mix. */
Blend stackingOf(BlendMode mode) {
  switch (mode) {
    case BlendMode::Normal:
      return Blend::Mix;
    case BlendMode::PlusLighter:
      return Blend::Add;
    case BlendMode::Multiply:
      return Blend::Multiply;
    default:
      reportOnce("over:" + std::string(name(mode)),
                 "over() stacks by normal, plus-lighter or multiply; " +
                     std::string(name(mode)) + " stacks as normal");
      return Blend::Mix;
  }
}

/** The blend's name as a recipe name spells it. */
std::string_view stackingName(Blend blend) {
  switch (blend) {
    case Blend::Mix:
      return "mix";
    case Blend::Add:
      return "add";
    case Blend::Multiply:
      return "multiply";
  }
  return "?";
}

/** The name every recipe of a stack by @p blend carries. */
std::string nameOfStack(Blend blend);

/** The three operands, in the order a composed body evaluates them: the
 *  slot each fills, the prefix its own names take among the composed
 *  recipe's, and the space its body stands in. */
struct Operand {
  const char* slot;
  const char* prefix;
  const char* space;
};
constexpr Operand kOperands[3] = {
    {"base", "base_", "overBase"},
    {"top", "top_", "overTop"},
    {"mask", "mask_", "overMask"},
};

std::string_view skslFile(Blend blend) {
  switch (blend) {
    case Blend::Add:
      return "OverAdd.sksl";
    case Blend::Multiply:
      return "OverMultiply.sksl";
    case Blend::Mix:
      return "OverMix.sksl";
  }
  return "OverMix.sksl";
}

/** The stack's SkSL body: the coverage every blend reads, then the blend. */
std::string skslBody(Blend blend) {
  return std::string(shaderSource("OverPrelude.sksl"))
      .append(shaderSource(skslFile(blend)));
}

std::string_view slangFile(Blend blend) {
  switch (blend) {
    case Blend::Add:
      return "OverAdd.slang";
    case Blend::Multiply:
      return "OverMultiply.slang";
    case Blend::Mix:
      return "OverMix.slang";
  }
  return "OverMix.slang";
}

std::shared_ptr<const Recipe> make(Blend blend) {
  return std::make_shared<const Recipe>(
      Recipe::of<OverParameters>(nameOfStack(blend))
          .slot("base")
          .slot("top")
          .slot("mask")
          .body(Target::SkSL, skslBody(blend)));
}

/** @p text with its comments taken out, so the composed body spells only
 *  what its code names: a slot or a lobe a comment mentions is not one
 *  the stack reads. The text carries no string a comment marker could
 *  stand inside. */
std::string uncommented(const std::string& text) {
  std::string out;
  out.reserve(text.size());
  for (size_t at = 0; at < text.size();) {
    if (text.compare(at, 2, "//") == 0) {
      const size_t end = text.find('\n', at);
      at = end == std::string::npos ? text.size() : end;
      continue;
    }
    if (text.compare(at, 2, "/*") == 0) {
      const size_t end = text.find("*/", at + 2);
      at = end == std::string::npos ? text.size() : end + 2;
      out += ' ';
      continue;
    }
    out += text[at++];
  }
  return out;
}

/** The names one operand's body spells that the composed recipe declares
 *  under a prefix: its parameters and the sampled slots its body reads.
 *  They are the one set another operand's declarations can collide with,
 *  because a frame input is spelled the same by every recipe and shared,
 *  and everything else a body names it defines itself. A slot the body
 *  never reads is left out, so the stack declares no slot an operand
 *  does not sample. */
std::vector<std::string> renamed(const Recipe& recipe) {
  std::vector<std::string> names;
  for (const Field& f : recipe.parameters().fields) names.push_back(f.name);
  const std::string body = uncommented(*recipe.body(Target::Slang));
  for (const std::string& slot : recipe.slots())
    if (detail::spellsIdentifier(body, slot)) names.push_back(slot);
  return names;
}

/** THE OPTIONAL LOBES, by the variable whose writing states each one and
 *  the mark its text stands between in the prelude. */
struct OverLobe {
  std::string_view variable;
  std::string_view mark;
};
constexpr OverLobe kOverLobes[] = {
    {"gSurfaceSheenColor", "sheen"},
    {"gSurfaceAnisotropy", "anisotropy"},
    {"gSurfaceIridescence", "iridescence"},
    {"gSurfaceSpecular", "specular"},
    {"gSurfaceTransmission", "transmission"},
    {"gSurfaceDistortion", "distortion"},
};

/** The text between `// <mark> {` and `// } <mark>`, its two marking
 *  lines included, taken out of @p text wherever it stands. */
void strike(std::string& text, std::string_view mark) {
  const std::string open = "// " + std::string(mark) + " {";
  const std::string close = "// } " + std::string(mark);
  for (size_t at = text.find(open); at != std::string::npos;
       at = text.find(open, at)) {
    const size_t start = text.rfind('\n', at);
    const size_t end = text.find(close, at);
    if (end == std::string::npos) return;
    const size_t after = text.find('\n', end);
    const size_t from = start == std::string::npos ? 0 : start;
    text.erase(from, (after == std::string::npos ? text.size() : after) - from);
  }
}

/** ONE OPERAND'S BODY, INLINED. Its uniforms and its slots are the
 *  composed recipe's, spelled with the operand's prefix; its helpers and
 *  its `surface` stand in a space of their own, so three operands over
 *  one recipe are three bodies and not one redefined twice.
 *
 *  The renaming is the preprocessor's rather than a rewrite of the text:
 *  a body names its parameters and its slots exactly as the recipe
 *  declares them, and a macro maps each without the body being touched —
 *  including through a body that was itself composed, whose own names
 *  are already prefixed and expand a second time. The one thing this
 *  cannot carry is a body that gives a LOCAL the name of one of its own
 *  parameters. */
std::string inlined(const Recipe& recipe, const Operand& operand) {
  const std::vector<std::string> names = renamed(recipe);
  std::string out;
  for (const std::string& name : names) {
    out += "#define ";
    out += name;
    out += ' ';
    out += operand.prefix;
    out += name;
    out += '\n';
  }
  out += "namespace ";
  out += operand.space;
  out += " {\n";
  out += *recipe.body(Target::Slang);
  out += "\n}\n";
  for (const std::string& name : names) {
    out += "#undef ";
    out += name;
    out += '\n';
  }
  return out;
}

/** The whole composed Slang body: the mixing helpers, the three operands
 *  under their own names, and the surface that asks all three and
 *  combines what they said. */
std::string composedSlang(Blend blend, const Recipe* operands[3]) {
  // A lobe is mixed by the mask only where an operand states it; a stack
  // of operands that state none carries none.
  std::string out(shaderSource("OverPrelude.slang"));
  for (const OverLobe& lobe : kOverLobes) {
    bool stated = false;
    for (int i = 0; i < 3; ++i)
      stated =
          stated || operands[i]->body(Target::Slang)->find(lobe.variable) !=
                        std::string::npos;
    if (!stated) strike(out, lobe.mark);
  }
  for (int i = 0; i < 3; ++i) out += inlined(*operands[i], kOperands[i]);
  out += shaderSource(slangFile(blend));
  return uncommented(out);
}

/** The composed recipe for @p blend over these three definitions: the
 *  operands' parameters and slots under a prefix each, the union of the
 *  frame inputs they read, the plain SkSL body — a target that samples
 *  its operands needs nothing composed — and the composed Slang one. */
std::shared_ptr<const Recipe> composeRecipe(Blend blend,
                                            const Recipe* operands[3]) {
  Schema parameters;
  parameters.fields.push_back({"amount", ParameterType::Float, 1, 0});
  parameters.byteSize = sizeof(float);
  for (int i = 0; i < 3; ++i)
    for (const Field& f : operands[i]->parameters().fields) {
      Field copy = f;
      copy.name = std::string(kOperands[i].prefix) + f.name;
      copy.offset = parameters.byteSize;
      parameters.byteSize += f.floats * sizeof(float);
      parameters.fields.push_back(std::move(copy));
    }

  Recipe recipe = Recipe::of(nameOfStack(blend), parameters);
  recipe.slot("base").slot("top").slot("mask");
  for (int i = 0; i < 3; ++i) {
    for (const std::string& slot : operands[i]->slots())
      recipe.slot(std::string(kOperands[i].prefix) + slot);
    for (FrameInput input :
         {FrameInput::Time, FrameInput::Resolution, FrameInput::ContentScale,
          FrameInput::WorldTransform, FrameInput::LocalToSample})
      if (operands[i]->reads(input)) recipe.frame(input);
  }
  recipe.body(Target::SkSL, skslBody(blend));
  recipe.body(Target::Slang, composedSlang(blend, operands));
  return std::make_shared<const Recipe>(std::move(recipe));
}

/** WHICH TARGETS A STACK MUST BE COMPOSED FOR: the ones whose compiler
 *  is handed one body per material and so cannot reach a child material
 *  at all. */
constexpr Target kComposedTargets[] = {Target::Slang};

/** What every stack's recipe name begins with. */
constexpr std::string_view kStackPrefix = "over.";

/** Whether @p recipeName is a stack's. Read off the NAME rather than off
 *  recipe identity, because a composed stack carries a recipe built for
 *  its own operands and no two of them are the same object; spelled as a
 *  prefix and no string built, because every walk down a stack asks this
 *  of every step. The prefix is this library's own and no recipe outside
 *  it is named under one. */
bool isStack(std::string_view recipeName) {
  return recipeName.starts_with(kStackPrefix);
}

/** The composed recipe for these three definitions, made once. Two
 *  stacks over the same three recipes are one definition, one program
 *  and one pipeline; a recipe here is a shared definition held for the
 *  life of the process, so the identity a cache key spells is stable.
 *  Null when there is nothing to compose for — no target that needs it
 *  has a compiler, or an operand has no body for one that does. */
std::shared_ptr<const Recipe> composed(
    Blend blend, const std::shared_ptr<const Recipe>& base,
    const std::shared_ptr<const Recipe>& top,
    const std::shared_ptr<const Recipe>& mask) {
  bool wanted = false;
  for (Target target : kComposedTargets) {
    // The operands are asked FIRST, because that answer is the recipes'
    // own and the compiler registry's is behind a lock every `over()`
    // would otherwise take: three definitions that carry no body for a
    // target settle it without asking anyone.
    if (!base->has(target) || !top->has(target) || !mask->has(target)) continue;
    if (!ProgramCache::shared().hasCompiler(target)) continue;
    wanted = true;
    break;
  }
  if (!wanted) return nullptr;

  // The operands are HELD in the key, not pointed at: a definition freed
  // and a second allocated at its address would otherwise inherit the
  // first's composition. Holding them also keeps the composed body's
  // sources alive for as long as the composition is cached.
  struct Key {
    Blend blend;
    std::shared_ptr<const Recipe> base;
    std::shared_ptr<const Recipe> top;
    std::shared_ptr<const Recipe> mask;
    auto operator<=>(const Key& other) const {
      if (auto c = blend <=> other.blend; c != 0) return c;
      if (auto c = base.get() <=> other.base.get(); c != 0) return c;
      if (auto c = top.get() <=> other.top.get(); c != 0) return c;
      return mask.get() <=> other.mask.get();
    }
  };
  static std::mutex mutex;
  static boost::container::flat_map<Key, std::shared_ptr<const Recipe>> built;
  const Key key{blend, base, top, mask};
  const std::lock_guard lock(mutex);
  auto it = built.find(key);
  if (it != built.end()) return it->second;
  const Recipe* operands[3] = {base.get(), top.get(), mask.get()};
  return built.emplace(key, composeRecipe(blend, operands)).first->second;
}

/** The operands' values and sampled slots written into @p out under the
 *  prefixes its recipe declares them with. */
void carry(Material& out, const Material* operands[3]) {
  for (int i = 0; i < 3; ++i) {
    const Material& operand = *operands[i];
    const std::string prefix = kOperands[i].prefix;
    for (const Field& f : operand.recipe().parameters().fields) {
      if (f.offset + f.floats * sizeof(float) > operand.bytes().size())
        continue;
      const auto* values =
          reinterpret_cast<const float*>(operand.bytes().data() + f.offset);
      out.set(prefix + f.name, std::span<const float>(values, f.floats));
    }
    // A slot filled with a MATERIAL is not carried across: the composed
    // recipe declares a sampled slot per slot, and a material in one is
    // a body the composition has already inlined — a stack of stacks
    // reaches here with its operands' slots already flattened.
    for (const auto& [slot, filled] : operand.slots())
      if (filled.leaf) out.slot(prefix + slot, filled.leaf);
  }
}

}  // namespace

namespace {

std::string nameOfStack(Blend blend) {
  return std::string(kStackPrefix) + std::string(stackingName(blend));
}

const std::shared_ptr<const Recipe>& recipeOf(Blend blend) {
  static const std::shared_ptr<const Recipe> mix = make(Blend::Mix);
  static const std::shared_ptr<const Recipe> add = make(Blend::Add);
  static const std::shared_ptr<const Recipe> multiply = make(Blend::Multiply);
  switch (blend) {
    case Blend::Add:
      return add;
    case Blend::Multiply:
      return multiply;
    case Blend::Mix:
      break;
  }
  return mix;
}

}  // namespace

std::string stackName(BlendMode blend) {
  return nameOfStack(stackingOf(blend));
}

const std::shared_ptr<const Recipe>& overRecipe(BlendMode blend) {
  return recipeOf(stackingOf(blend));
}

Material over(Material base, Material top, Material mask, BlendMode mode,
              float amount) {
  const Blend blend = stackingOf(mode);
  const std::shared_ptr<const Recipe> recipe = composed(
      blend, base.recipePointer(), top.recipePointer(), mask.recipePointer());
  const auto fill = [&](Material out) {
    out.slot("base", std::move(base));
    out.slot("top", std::move(top));
    out.slot("mask", std::move(mask));
    return out;
  };
  if (!recipe) return fill(Material(recipeOf(blend), OverParameters{amount}));

  // A composed recipe's ABI is its operands' fields and not one struct's,
  // so its values are written field by field rather than poured from a
  // parameter struct.
  Material out(recipe);
  out.set("amount", amount);
  const Material* operands[3] = {&base, &top, &mask};
  carry(out, operands);
  return fill(std::move(out));
}

const Material* under(const Material& material) {
  if (!isStack(material.recipe().name())) return &material;
  if (const Material* base = material.slot("base")) return base;
  return &material;
}

int stackDepth(const Material& material) {
  int depth = 0;
  for (const Material* p = &material; under(*p) != p; p = under(*p)) ++depth;
  return depth;
}

}  // namespace sigil::material
