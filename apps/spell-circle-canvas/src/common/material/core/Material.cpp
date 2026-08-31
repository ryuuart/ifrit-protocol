/** @file
 * Material instances: byte mirroring of the params struct, per-field
 * writes and bindings, child slots holding materials or leaves, the tier
 * queries, value equality and the memoised resolve.
 */

#include "sigilmaterial/core/Material.h"

#include <algorithm>
#include <cmath>
#include <cstring>

namespace sigil::material {

Material::Material(std::shared_ptr<const Recipe> recipe)
    : m_recipe(std::move(recipe)), m_bytes(m_recipe->params().byteSize) {}

Material::Material(std::shared_ptr<const Recipe> recipe, const void* params,
                   size_t size, const Schema* schema)
    : Material(std::move(recipe)) {
  write(params, size, schema);
}

Material Material::withRecipe(std::shared_ptr<const Recipe> recipe) const {
  if (!recipe || recipe->params() != m_recipe->params()) {
    reportOnce("specialize:" + m_recipe->name(),
               "recipe \"" + m_recipe->name() +
                   "\": a specialization must carry the same params layout; "
                   "the material stays on its own recipe");
    return *this;
  }
  Material out = *this;
  out.m_recipe = std::move(recipe);
  // The memo keys on the bytes, the target and the variant — not on the
  // recipe — so a specialization that inherited it would hand back the
  // other definition's program.
  out.m_memo = {};
  return out;
}

void Material::write(const void* params, size_t size, const Schema* schema) {
  // A params struct with no fields lays out to nothing while still
  // occupying a byte as a C++ object, so its size can never be the
  // upload's; there is simply nothing to copy.
  if (*schema == m_recipe->params() && schema->fields.empty()) return;
  if (*schema != m_recipe->params() || size != m_bytes.size()) {
    reportOnce("params:" + m_recipe->name(),
               "recipe \"" + m_recipe->name() +
                   "\": the params struct given is not the one the recipe "
                   "was defined over; the values are ignored");
    return;
  }
  std::memcpy(m_bytes.data(), params, size);
}

void Material::write(std::string_view name, Kind kind, const void* floats,
                     size_t count) {
  const Field* f = m_recipe->params().find(name);
  const std::string key = "field:" + m_recipe->name() + ":" + std::string(name);
  if (!f) {
    reportOnce(key, "recipe \"" + m_recipe->name() + "\" declares no field \"" +
                        std::string(name) + "\"; the value is ignored");
    return;
  }
  // Colour and float4 interchange — both are four floats and the shader
  // declares one float4 — but a count mismatch means a different uniform.
  if (f->floats != count) {
    reportOnce(key, "recipe \"" + m_recipe->name() + "\" field \"" +
                        std::string(name) + "\" spans " +
                        std::to_string(f->floats) + " floats, not " +
                        std::to_string(count) + "; the value is ignored");
    return;
  }
  (void)kind;
  std::memcpy(m_bytes.data() + f->offset, floats, count * sizeof(float));
}

Material::Binding* Material::binding(std::string_view name) {
  for (Binding& b : m_bindings)
    if (b.name == name) return &b;
  return nullptr;
}

Material& Material::bind(std::string_view name,
                         const choreograph::Output<float>* output) {
  const Field* f = m_recipe->params().find(name);
  if (!f || f->kind != Kind::Float) {
    reportOnce("bind:" + m_recipe->name() + ":" + std::string(name),
               "recipe \"" + m_recipe->name() + "\" has no float field \"" +
                   std::string(name) + "\" to bind an output to");
    return *this;
  }
  if (Binding* b = binding(name)) {
    b->output = output;
    b->block = nullptr;
    if (!output)
      std::erase_if(m_bindings,
                    [&](const Binding& x) { return x.name == name; });
    return *this;
  }
  if (output) m_bindings.push_back({std::string(name), output, nullptr});
  return *this;
}

Material& Material::bind(std::string_view name,
                         std::shared_ptr<const UniformBlock> block) {
  const Field* f = m_recipe->params().find(name);
  const std::string key = "bind:" + m_recipe->name() + ":" + std::string(name);
  if (!f || f->kind != Kind::FloatArray) {
    reportOnce(key, "recipe \"" + m_recipe->name() +
                        "\" has no array field \"" + std::string(name) +
                        "\" to bind a block to");
    return *this;
  }
  if (block && block->size() != f->floats) {
    reportOnce(key, "recipe \"" + m_recipe->name() + "\" field \"" +
                        std::string(name) + "\" holds " +
                        std::to_string(f->floats) +
                        " floats; the block holds " +
                        std::to_string(block->size()) + " and is ignored");
    return *this;
  }
  if (Binding* b = binding(name)) {
    b->output = nullptr;
    b->block = std::move(block);
    if (!b->block)
      std::erase_if(m_bindings,
                    [&](const Binding& x) { return x.name == name; });
    return *this;
  }
  if (block)
    m_bindings.push_back({std::string(name), nullptr, std::move(block)});
  return *this;
}

void Material::place(std::string_view name, Slot slot) {
  const auto slots = m_recipe->children();
  if (std::find(slots.begin(), slots.end(), name) == slots.end()) {
    reportOnce("child:" + m_recipe->name() + ":" + std::string(name),
               "recipe \"" + m_recipe->name() + "\" declares no child slot \"" +
                   std::string(name) + "\"; the child is ignored");
    return;
  }
  for (auto& [existing, s] : m_children) {
    if (existing == name) {
      s = std::move(slot);
      return;
    }
  }
  m_children.emplace_back(std::string(name), std::move(slot));
  // Recipe order, so two materials filling the same slots in different
  // orders compare equal.
  std::sort(m_children.begin(), m_children.end(),
            [&](const auto& a, const auto& b) {
              return std::find(slots.begin(), slots.end(), a.first) <
                     std::find(slots.begin(), slots.end(), b.first);
            });
}

Material& Material::child(std::string_view name, Material material) {
  place(name, {std::make_shared<const Material>(std::move(material)), nullptr});
  return *this;
}

Material& Material::child(std::string_view name,
                          std::shared_ptr<const Leaf> leaf) {
  place(name, {nullptr, std::move(leaf)});
  return *this;
}

const Material* Material::child(std::string_view name) const {
  for (const auto& [slot, s] : m_children)
    if (slot == name) return s.material.get();
  return nullptr;
}

const Leaf* Material::leaf(std::string_view name) const {
  for (const auto& [slot, s] : m_children)
    if (slot == name) return s.leaf.get();
  return nullptr;
}

Material& Material::amount(float a01) {
  m_amount = std::clamp(a01, 0.0f, 1.0f);
  return *this;
}

Material& Material::quantizeTime(float hz) {
  m_quantizeHz = hz > 0.0f ? hz : 0.0f;
  return *this;
}

Material& Material::worldSpace(bool on) {
  m_worldSpace = on;
  return *this;
}

bool Material::isAnimated() const {
  if (!m_bindings.empty()) return true;
  if (m_recipe->reads(FrameInput::Time) ||
      m_recipe->reads(FrameInput::ContentScale))
    return true;
  for (const auto& [slot, s] : m_children) {
    if (s.material && s.material->isAnimated()) return true;
    if (s.leaf && s.leaf->animated()) return true;
  }
  return false;
}

bool Material::geometryDependent() const {
  if (m_recipe->reads(FrameInput::Resolution) ||
      m_recipe->reads(FrameInput::WorldTransform))
    return true;
  for (const auto& [slot, s] : m_children)
    if (s.material && s.material->geometryDependent()) return true;
  return false;
}

bool Material::operator==(const Material& other) const {
  if (m_recipe != other.m_recipe || m_bytes != other.m_bytes ||
      m_amount != other.m_amount || m_quantizeHz != other.m_quantizeHz ||
      m_worldSpace != other.m_worldSpace ||
      m_bindings.size() != other.m_bindings.size() ||
      m_children.size() != other.m_children.size())
    return false;
  for (const Binding& a : m_bindings) {
    const Binding* b = nullptr;
    for (const Binding& x : other.m_bindings)
      if (x.name == a.name) b = &x;
    if (!b || a.output != b->output || a.block != b->block) return false;
  }
  for (size_t i = 0; i < m_children.size(); ++i) {
    const auto& [slot, s] = m_children[i];
    const auto& [otherSlot, o] = other.m_children[i];
    if (slot != otherSlot) return false;
    if ((s.material != nullptr) != (o.material != nullptr)) return false;
    if (s.material && !(*s.material == *o.material)) return false;
    if (s.leaf && !(*s.leaf == *o.leaf)) return false;
  }
  return true;
}

Material::Resolved Material::resolve(Target target, const FrameData& frame,
                                     Variant variant) const {
  const Schema& layout = m_recipe->layout();
  m_scratch.assign(layout.byteSize, std::byte{0});
  if (!m_bytes.empty())
    std::memcpy(m_scratch.data(), m_bytes.data(), m_bytes.size());
  for (const Binding& b : m_bindings) {
    const Field* f = m_recipe->params().find(b.name);
    if (!f) continue;
    if (b.output) {
      const float v = b.output->value();
      std::memcpy(m_scratch.data() + f->offset, &v, sizeof(float));
    } else if (b.block) {
      const std::span<const float> values = b.block->values();
      std::memcpy(m_scratch.data() + f->offset, values.data(),
                  values.size() * sizeof(float));
    }
  }
  const auto put = [&](FrameInput input, const void* floats, size_t count) {
    if (!m_recipe->reads(input)) return;
    const Field* f = layout.find(uniformName(input));
    std::memcpy(m_scratch.data() + f->offset, floats, count * sizeof(float));
  };
  float seconds = (float)frame.seconds;
  if (m_quantizeHz > 0.0f)
    seconds = std::floor(seconds * m_quantizeHz) / m_quantizeHz;
  put(FrameInput::Time, &seconds, 1);
  put(FrameInput::Resolution, &frame.resolution, 2);
  put(FrameInput::ContentScale, &frame.contentScale, 1);
  put(FrameInput::WorldTransform, &frame.world, 9);

  if (m_memo.valid && m_memo.target == target && m_memo.variant == variant &&
      m_memo.bytes == m_scratch && m_memo.program)
    return {m_memo.program, m_memo.bytes};
  m_memo.valid = true;
  m_memo.target = target;
  m_memo.variant = variant;
  m_memo.bytes.swap(m_scratch);
  m_memo.program = program(m_recipe, target, variant);
  return {m_memo.program, m_memo.bytes};
}

}  // namespace sigil::material
