/** @file
 * Slot children written into one Slang body: the composed definition, made
 * once per set of definitions, and the instance carried into it with its
 * values, its bindings and its leaves.
 */

#include <sigilmaterial/advanced/Program.h>
#include <sigilmaterial/advanced/Recipe.h>
#include <sigilmaterial/core/Material.h>

#include <boost/container/flat_map.hpp>
#include <cctype>
#include <cstring>
#include <memory>
#include <mutex>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace sigil::material {

namespace {

/** One slot whose child is written into the body: the slot's name and the
 *  child's (already inlined) definition. */
struct Inlined {
  std::string slot;
  std::shared_ptr<const Recipe> recipe;
  bool operator==(const Inlined& other) const {
    return slot == other.slot && recipe == other.recipe;
  }
};

/** Whether @p body reads the slot @p slot as an image is read — the slot's
 *  name, as a whole identifier, followed by a sample. A body that names
 *  the slot only in a comment, or reads it under names of its own, does
 *  not. */
bool readsSlot(std::string_view body, std::string_view slot) {
  const auto identifier = [](char c) {
    return std::isalnum((unsigned char)c) != 0 || c == '_';
  };
  for (size_t at = body.find(slot); at != std::string_view::npos;
       at = body.find(slot, at + 1)) {
    if (at > 0 && identifier(body[at - 1])) continue;
    size_t next = at + slot.size();
    if (next < body.size() && identifier(body[next])) continue;
    while (next < body.size() && std::isspace((unsigned char)body[next]))
      ++next;
    if (next < body.size() && body[next] == '.' &&
        body.substr(next + 1).starts_with("Sample"))
      return true;
  }
  return false;
}

/** The prefix a child's names take: the slot it fills. */
std::string prefixOf(const std::string& slot) { return slot + "_"; }

/** ONE CHILD'S BODY, INLINED under its slot's name: its parameters and
 *  slots renamed by the preprocessor, its helpers and its `surface` in a
 *  namespace of their own, and the slot itself answered by a value whose
 *  reads call that `surface` — so the parent's body reads the slot exactly
 *  as it reads an image. */
std::string inlinedBody(const Inlined& child) {
  const Recipe& recipe = *child.recipe;
  std::vector<std::string> names;
  for (const Field& field : recipe.parameters().fields)
    names.push_back(field.name);
  for (const std::string& slot : recipe.slots()) names.push_back(slot);
  const std::string prefix = prefixOf(child.slot);
  const std::string space = "inlined_" + child.slot;
  std::string out;
  for (const std::string& name : names)
    out += "#define " + name + " " + prefix + name + "\n";
  out += "namespace " + space + " {\n" + *recipe.body(Target::Slang) + "\n}\n";
  for (const std::string& name : names) out += "#undef " + name + "\n";
  const std::string type = "InlinedSlot_" + child.slot;
  out += "struct " + type + " {\n";
  out += "  float4 Sample(float2 uv) { return " + space + "::surface(uv); }\n";
  out += "  float4 SampleLevel(float2 uv, float level) { return " + space +
         "::surface(uv); }\n";
  out += "};\n";
  out += "static " + type + " " + child.slot + ";\n";
  return out;
}

/** THE COMPOSED DEFINITION: the parent's parameters where they were, the
 *  children's after them under their prefixes; the parent's leaf slots
 *  and the children's, prefixed; every frame input any of them reads; and
 *  the one Slang body. */
std::shared_ptr<const Recipe> composeRecipe(
    const Recipe& parent, const std::vector<Inlined>& children) {
  Schema parameters = parent.parameters();
  for (const Inlined& child : children)
    for (const Field& field : child.recipe->parameters().fields) {
      Field copy = field;
      copy.name = prefixOf(child.slot) + field.name;
      copy.offset = parameters.byteSize;
      parameters.byteSize += field.floats * sizeof(float);
      parameters.fields.push_back(std::move(copy));
    }
  Recipe recipe = Recipe::of(parent.name() + ".inlined", parameters);
  for (const std::string& slot : parent.slots()) {
    bool filled = false;
    for (const Inlined& child : children) filled = filled || child.slot == slot;
    if (!filled) recipe.slot(slot);
  }
  std::string body;
  for (const Inlined& child : children) {
    for (const std::string& slot : child.recipe->slots())
      recipe.slot(prefixOf(child.slot) + slot);
    body += inlinedBody(child);
  }
  for (FrameInput input :
       {FrameInput::Time, FrameInput::Resolution, FrameInput::ContentScale,
        FrameInput::WorldTransform, FrameInput::LocalToSample}) {
    bool reads = parent.reads(input);
    for (const Inlined& child : children)
      reads = reads || child.recipe->reads(input);
    if (reads) recipe.frame(input);
  }
  body += *parent.body(Target::Slang);
  recipe.body(Target::Slang, std::move(body));
  return std::make_shared<const Recipe>(std::move(recipe));
}

/** The composed definition for this parent over these children, made
 *  once. The definitions are HELD in the key, so one freed and another
 *  allocated at its address cannot inherit its composition. */
std::shared_ptr<const Recipe> composed(
    const std::shared_ptr<const Recipe>& parent,
    const std::vector<Inlined>& children) {
  struct Key {
    std::shared_ptr<const Recipe> parent;
    std::vector<Inlined> children;
    bool operator<(const Key& other) const {
      if (parent.get() != other.parent.get())
        return parent.get() < other.parent.get();
      if (children.size() != other.children.size())
        return children.size() < other.children.size();
      for (size_t i = 0; i < children.size(); ++i) {
        if (children[i].slot != other.children[i].slot)
          return children[i].slot < other.children[i].slot;
        if (children[i].recipe.get() != other.children[i].recipe.get())
          return children[i].recipe.get() < other.children[i].recipe.get();
      }
      return false;
    }
  };
  static std::mutex mutex;
  static boost::container::flat_map<Key, std::shared_ptr<const Recipe>> built;
  Key key{parent, children};
  const std::lock_guard lock(mutex);
  const auto found = built.find(key);
  if (found != built.end()) return found->second;
  std::shared_ptr<const Recipe> made = composeRecipe(*parent, children);
  built.emplace(std::move(key), made);
  return made;
}

}  // namespace

Material inlined(const Material& material) {
  if (!material.m_recipe || !material.m_recipe->has(Target::Slang))
    return material;
  // The children first, so a child that holds materials of its own
  // arrives here as one body already.
  std::vector<Inlined> children;
  std::vector<Material> instances;
  for (const auto& [slot, filled] : material.m_slots) {
    // Only a slot the Slang body READS: a definition composed for this
    // target already — a stack — keeps its operands in their slots for
    // the other targets and reads them inlined under names of its own.
    if (!filled.material ||
        !readsSlot(*material.m_recipe->body(Target::Slang), slot))
      continue;
    Material child = inlined(*filled.material);
    if (!child.m_recipe || !child.m_recipe->has(Target::Slang) ||
        child.isComposed())
      continue;
    children.push_back({slot, child.m_recipe});
    instances.push_back(std::move(child));
  }
  if (children.empty()) return material;

  Material out(composed(material.m_recipe, children));
  // THE VALUES, as bytes: the parent's where the parent had them, each
  // child's after them where the composed layout put its prefix.
  std::memcpy(out.m_bytes.data(), material.m_bytes.data(),
              std::min(out.m_bytes.size(), material.m_bytes.size()));
  const Schema& layout = out.m_recipe->parameters();
  out.m_bindings = material.m_bindings;
  for (size_t i = 0; i < children.size(); ++i) {
    const Material& child = instances[i];
    const std::string prefix = prefixOf(children[i].slot);
    for (const Field& field : child.m_recipe->parameters().fields) {
      const Field* placed = layout.find(prefix + field.name);
      if (!placed ||
          field.offset + field.floats * sizeof(float) > child.m_bytes.size())
        continue;
      std::memcpy(out.m_bytes.data() + placed->offset,
                  child.m_bytes.data() + field.offset,
                  field.floats * sizeof(float));
    }
    // A LIVE CHILD STAYS LIVE: its bindings follow the same outputs under
    // the prefixed names, so a frame a sketch drives reaches the body.
    for (const auto& binding : child.m_bindings) {
      auto carried = binding;
      carried.name = prefix + binding.name;
      out.m_bindings.push_back(std::move(carried));
    }
    for (const auto& [slot, filled] : child.m_slots)
      if (filled.leaf) out.m_slots.push_back({prefix + slot, filled});
  }
  for (const auto& [slot, filled] : material.m_slots)
    if (filled.leaf) out.m_slots.push_back({slot, filled});
  out.m_amount = material.m_amount;
  out.m_quantizeHz = material.m_quantizeHz;
  out.m_worldSpace = material.m_worldSpace;
  return out;
}

}  // namespace sigil::material
