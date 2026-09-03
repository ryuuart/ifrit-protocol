/** @file
 * The three stacking bodies in SkSL, the composition that writes the
 * same three readings in Slang out of the operands' own bodies, and the
 * builder that fills the slots either way.
 */

#include "sigilmaterial/core/Combine.h"

#include <sigilmaterial/core/Program.h>

#include <map>
#include <mutex>
#include <span>
#include <string>
#include <utility>
#include <vector>

namespace sigil::material {

namespace {

/** The mask read: red, scaled by the amount, clamped into 0..1. */
constexpr char kPrelude[] = R"(
half coverage(float2 xy) {
  return half(clamp(float(mask.eval(xy).r) * amount, 0.0, 1.0));
}
)";

constexpr char kMix[] = R"(
half4 main(float2 xy) {
  return mix(base.eval(xy), top.eval(xy), coverage(xy));
}
)";

constexpr char kAdd[] = R"(
half4 main(float2 xy) {
  return base.eval(xy) + top.eval(xy) * coverage(xy);
}
)";

constexpr char kMultiply[] = R"(
half4 main(float2 xy) {
  half4 b = base.eval(xy);
  return mix(b, b * top.eval(xy), coverage(xy));
}
)";

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

/** What a composed body says beyond a colour, taken from the scaffold's
 *  own per-pixel variables after one operand has run, and the mixing of
 *  two operands' answers by the coverage that mixes their colours. The
 *  variables themselves belong to the renderer, so this text names them
 *  and declares none of them. */
constexpr char kSlangPrelude[] = R"(
struct OverTerms {
  float3 normal;
  float gloss;
  float metal;
  bool perPixel;
};

/** The per-pixel variables back at what "said nothing" means, so each
 *  operand is asked with a clean slate and the stack answers for all
 *  three of them rather than for whichever ran last. */
void overClear() {
  gSurfaceNormal = float3(0.0, 0.0, 1.0);
  gSurfaceGloss = -1.0;
  gSurfaceMetal = 0.0;
  gSurfacePerPixel = false;
}

OverTerms overTaken() {
  OverTerms out;
  out.normal = gSurfaceNormal;
  out.gloss = gSurfaceGloss;
  out.metal = gSurfaceMetal;
  out.perPixel = gSurfacePerPixel;
  return out;
}

// A Blinn exponent below zero is "take the scaffold's own", which is not
// a number to interpolate toward: where one operand named an exponent
// and the other did not, the one that named it stands.
float overGloss(float a, float b, float t) {
  if (a < 0.0) return b;
  if (b < 0.0) return a;
  return a + (b - a) * t;
}

// Written out rather than taken from the language's library, for the
// reason the portable subset exists: an intrinsic whose two targets are
// two different pieces of code is where one source stops producing one
// answer.
float4 overMixed(float4 a, float4 b, float t) { return a + (b - a) * t; }

/** The two operands' per-pixel terms through the coverage that mixes
 *  their colours, so a top wearing a normal map bumps the surface only
 *  where the mask lets the top show. */
void overSay(OverTerms a, OverTerms b, float t) {
  if (!a.perPixel && !b.perPixel) return;
  gSurfaceNormal = normalizeP(a.normal + (b.normal - a.normal) * t);
  gSurfaceGloss = overGloss(a.gloss, b.gloss, t);
  gSurfaceMetal = a.metal + (b.metal - a.metal) * t;
  gSurfacePerPixel = true;
}
)";

/** The composed reading, with the blend's own last line. Straight alpha
 *  both sides, which is what a Slang body answers with. */
constexpr char kSlangMix[] = "  return overMixed(b, t, cov);\n";
constexpr char kSlangAdd[] = "  return b + t * cov;\n";
constexpr char kSlangMultiply[] = "  return overMixed(b, b * t, cov);\n";

const char* skslBody(Blend blend) {
  switch (blend) {
    case Blend::Add:
      return kAdd;
    case Blend::Multiply:
      return kMultiply;
    case Blend::Mix:
      break;
  }
  return kMix;
}

const char* slangTail(Blend blend) {
  switch (blend) {
    case Blend::Add:
      return kSlangAdd;
    case Blend::Multiply:
      return kSlangMultiply;
    case Blend::Mix:
      break;
  }
  return kSlangMix;
}

std::shared_ptr<const Recipe> make(Blend blend, const char* body) {
  return std::make_shared<const Recipe>(
      Recipe::of<OverParams>(stackName(blend))
          .child("base")
          .child("top")
          .child("mask")
          .body(Target::SkSL, std::string(kPrelude) + body));
}

/** The names one operand's body spells that the composed recipe declares
 *  under a prefix: its parameters and its sampled slots. They are the
 *  one set another operand's declarations can collide with, because a
 *  frame input is spelled the same by every recipe and shared, and
 *  everything else a body names it defines itself. */
std::vector<std::string> renamed(const Recipe& recipe) {
  std::vector<std::string> names;
  for (const Field& f : recipe.params().fields) names.push_back(f.name);
  for (const std::string& slot : recipe.children()) names.push_back(slot);
  return names;
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
  std::string out(kSlangPrelude);
  for (int i = 0; i < 3; ++i) out += inlined(*operands[i], kOperands[i]);
  out += R"(
float4 surface(float2 uv) {
  overClear();
  float4 b = overBase::surface(uv);
  OverTerms under = overTaken();
  overClear();
  float4 t = overTop::surface(uv);
  OverTerms upper = overTaken();
  overClear();
  float4 k = overMask::surface(uv);
  overClear();
  float cov = min(max(k.r * amount, 0.0), 1.0);
  overSay(under, upper, cov);
)";
  out += slangTail(blend);
  out += "}\n";
  return out;
}

/** The composed recipe for @p blend over these three definitions: the
 *  operands' parameters and slots under a prefix each, the union of the
 *  frame inputs they read, the plain SkSL body — a target that samples
 *  its operands needs nothing composed — and the composed Slang one. */
std::shared_ptr<const Recipe> composeRecipe(Blend blend,
                                            const Recipe* operands[3]) {
  Schema params;
  params.fields.push_back({"amount", Kind::Float, 1, 0});
  params.byteSize = sizeof(float);
  for (int i = 0; i < 3; ++i)
    for (const Field& f : operands[i]->params().fields) {
      Field copy = f;
      copy.name = std::string(kOperands[i].prefix) + f.name;
      copy.offset = params.byteSize;
      params.byteSize += f.floats * sizeof(float);
      params.fields.push_back(std::move(copy));
    }

  Recipe recipe = Recipe::of(stackName(blend), params);
  recipe.child("base").child("top").child("mask");
  for (int i = 0; i < 3; ++i) {
    for (const std::string& slot : operands[i]->children())
      recipe.child(std::string(kOperands[i].prefix) + slot);
    for (FrameInput input :
         {FrameInput::Time, FrameInput::Resolution, FrameInput::ContentScale,
          FrameInput::WorldTransform})
      if (operands[i]->reads(input)) recipe.frame(input);
  }
  recipe.body(Target::SkSL, std::string(kPrelude) + skslBody(blend));
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
std::shared_ptr<const Recipe> composed(Blend blend, const Recipe* base,
                                       const Recipe* top, const Recipe* mask) {
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

  struct Key {
    Blend blend;
    const Recipe* base;
    const Recipe* top;
    const Recipe* mask;
    auto operator<=>(const Key&) const = default;
  };
  static std::mutex mutex;
  static std::map<Key, std::shared_ptr<const Recipe>> built;
  const Key key{blend, base, top, mask};
  const std::lock_guard lock(mutex);
  auto it = built.find(key);
  if (it != built.end()) return it->second;
  const Recipe* operands[3] = {base, top, mask};
  return built.emplace(key, composeRecipe(blend, operands)).first->second;
}

/** The operands' values and sampled slots written into @p out under the
 *  prefixes its recipe declares them with. */
void carry(Material& out, const Material* operands[3]) {
  for (int i = 0; i < 3; ++i) {
    const Material& operand = *operands[i];
    const std::string prefix = kOperands[i].prefix;
    for (const Field& f : operand.recipe().params().fields) {
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
    for (const auto& [slot, filled] : operand.children())
      if (filled.leaf) out.child(prefix + slot, filled.leaf);
  }
}

}  // namespace

std::string_view name(Blend blend) {
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

std::string stackName(Blend blend) {
  return std::string(kStackPrefix) + std::string(name(blend));
}

const std::shared_ptr<const Recipe>& overRecipe(Blend blend) {
  static const std::shared_ptr<const Recipe> mix = make(Blend::Mix, kMix);
  static const std::shared_ptr<const Recipe> add = make(Blend::Add, kAdd);
  static const std::shared_ptr<const Recipe> multiply =
      make(Blend::Multiply, kMultiply);
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

Material over(Material base, Material top, Material mask, Blend blend,
              float amount) {
  const std::shared_ptr<const Recipe> recipe =
      composed(blend, &base.recipe(), &top.recipe(), &mask.recipe());
  const auto fill = [&](Material out) {
    out.child("base", std::move(base));
    out.child("top", std::move(top));
    out.child("mask", std::move(mask));
    return out;
  };
  if (!recipe) return fill(Material(overRecipe(blend), OverParams{amount}));

  // A composed recipe's ABI is its operands' fields and not one struct's,
  // so its values are written field by field rather than poured from a
  // params struct.
  Material out(recipe);
  out.set("amount", amount);
  const Material* operands[3] = {&base, &top, &mask};
  carry(out, operands);
  return fill(std::move(out));
}

const Material* under(const Material& m) {
  if (!isStack(m.recipe().name())) return &m;
  if (const Material* base = m.child("base")) return base;
  return &m;
}

int stackDepth(const Material& m) {
  int depth = 0;
  for (const Material* p = &m; under(*p) != p; p = under(*p)) ++depth;
  return depth;
}

}  // namespace sigil::material
