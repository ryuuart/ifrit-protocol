/** @file
 * Material instances: byte mirroring of the parameter struct, per-field
 * writes and bindings, slots holding materials or leaves, the tier
 * queries, value equality and the memoised resolve.
 */

#include "sigilmaterial/core/Material.h"

#include <sigilmaterial/advanced/FrameData.h>
#include <sigilmaterial/advanced/Leaf.h>
#include <sigilmaterial/advanced/Program.h>
#include <sigilmaterial/advanced/Recipe.h>
#include <sigilmaterial/advanced/UniformBlock.h>
#include <sigilmotion/values/Animatable.h>
#include <sigilmotion/values/Time.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstring>
#include <mutex>

namespace sigil::material {

namespace {

template <class Predicate>
bool surfaceChannelsMatch(const SurfaceOptions& surface, Predicate matches) {
  for (const Channel* channel :
       {&surface.metallic, &surface.roughness, &surface.occlusion})
    if (const auto* material = std::get_if<Material>(channel);
        material && matches(*material))
      return true;
  return (surface.normal && matches(*surface.normal)) ||
         (surface.emissionMap && matches(*surface.emissionMap));
}

}  // namespace

/** Everything beyond a bare program: the base when it is not a program,
 *  the layer stack, the surface and the effects. */
struct Material::Composition {
  std::optional<Color> color;
  std::shared_ptr<const detail::Part> source;
  std::vector<Layer> layers;
  std::optional<SurfaceOptions> surface;
  std::shared_ptr<const detail::Part> effects;
};

/** The sampled upload belongs to its readers. Only an upload held by
 *  this cache alone can be rewritten; outstanding results keep their
 *  bytes while another frame is resolved. */
struct Material::ResolveState {
  struct Upload {
    bool valid = false;
    Target target{};
    Variant variant{};
    std::shared_ptr<std::vector<std::byte>> bytes;
    std::shared_ptr<Program> program;
  };
  std::mutex mutex;
  std::vector<std::byte> scratch;
  std::array<Upload, 2> uploads;
  size_t latest = 0;
};

namespace {

bool samePart(const std::shared_ptr<const detail::Part>& a,
              const std::shared_ptr<const detail::Part>& b) {
  if (a == b) return true;
  return a && b && a->equals(*b);
}

}  // namespace

Material::Material(Color color) {
  auto composition = std::make_shared<Composition>();
  composition->color = color;
  m_composition = std::move(composition);
}

Material::Material(std::shared_ptr<const detail::Part> source) {
  auto composition = std::make_shared<Composition>();
  composition->source = std::move(source);
  m_composition = std::move(composition);
}

Material::Material(const MaterialParts& parts) : Material(parts.base) {
  for (const Layer& layer : parts.layers)
    this->layer(layer.source, layer.options);
  if (parts.surface) surface(*parts.surface);
  if (parts.effects.part()) placeEffects(parts.effects.part());
}

Material::Material(std::shared_ptr<const Recipe> recipe)
    : m_recipe(std::move(recipe)),
      m_bytes(m_recipe->parameters().byteSize),
      m_resolve(std::make_shared<ResolveState>()) {}

const Recipe& Material::recipe() const {
  if (m_recipe) return *m_recipe;
  struct Nothing {};
  static const Recipe nothing = Recipe::of<Nothing>("material.none");
  return nothing;
}

Material::Composition& Material::compose() {
  // Copy on write: a material is a value, and a copy shares its parts.
  auto copy = m_composition ? std::make_shared<Composition>(*m_composition)
                            : std::make_shared<Composition>();
  Composition& edited = *copy;
  m_composition = std::move(copy);
  return edited;
}

Material& Material::layer(Material source) {
  return layer(std::move(source), LayerOptions{});
}

Material& Material::layer(Material source, const LayerOptions& options) {
  LayerOptions clamped = options;
  clamped.opacity = std::clamp(clamped.opacity, 0.0f, 1.0f);
  compose().layers.push_back(Layer{std::move(source), std::move(clamped)});
  return *this;
}

Material& Material::surface(const SurfaceOptions& options) {
  compose().surface = options;
  return *this;
}

void Material::placeEffects(std::shared_ptr<const detail::Part> effects) {
  compose().effects = std::move(effects);
}

const detail::Part* Material::effectsPart() const {
  return m_composition ? m_composition->effects.get() : nullptr;
}

const Color* Material::color() const {
  return m_composition && m_composition->color ? &*m_composition->color
                                               : nullptr;
}

const detail::Part* Material::source() const {
  return m_composition ? m_composition->source.get() : nullptr;
}

std::span<const Layer> Material::layers() const {
  if (!m_composition) return {};
  return m_composition->layers;
}

const SurfaceOptions* Material::surface() const {
  return m_composition && m_composition->surface ? &*m_composition->surface
                                                 : nullptr;
}

Material Material::base() const {
  Material out = *this;
  if (!m_composition) return out;
  if (m_recipe) {
    out.m_composition = nullptr;
    return out;
  }
  auto bare = std::make_shared<Composition>();
  bare->color = m_composition->color;
  bare->source = m_composition->source;
  out.m_composition = std::move(bare);
  return out;
}

namespace {

/** A program-only operation on a material whose base is not a program. */
bool refuseWithoutProgram(const std::shared_ptr<const Recipe>& recipe,
                          std::string_view what, std::string_view name) {
  if (recipe) return false;
  reportOnce("noprogram:" + std::string(what),
             std::string(what) + " \"" + std::string(name) +
                 "\" on a material whose base is not a program is ignored");
  return true;
}

}  // namespace

Material::Material(std::shared_ptr<const Recipe> recipe, const void* parameters,
                   size_t size, const Schema* schema)
    : Material(std::move(recipe)) {
  write(parameters, size, schema);
}

Material Material::withRecipe(std::shared_ptr<const Recipe> recipe) const {
  if (!m_recipe) return *this;
  if (!recipe || recipe->parameters() != m_recipe->parameters()) {
    reportOnce(
        "specialize:" + m_recipe->name(),
        "recipe \"" + m_recipe->name() +
            "\": a specialization must carry the same parameters layout; "
            "the material stays on its own recipe");
    return *this;
  }
  Material out = *this;
  out.m_recipe = std::move(recipe);
  // The memo keys on the bytes, the target and the variant — not on the
  // recipe — so a specialization that inherited it would hand back the
  // other definition's program.
  out.m_resolve = std::make_shared<ResolveState>();
  return out;
}

bool Material::writePartInput(std::string_view name,
                              std::span<const float> values) {
  if (m_recipe || !m_composition || !m_composition->source) return false;
  std::shared_ptr<const detail::Part> next =
      m_composition->source->withInput(name, values);
  if (!next) return false;
  compose().source = std::move(next);
  return true;
}

void Material::write(const void* parameters, size_t size,
                     const Schema* schema) {
  if (refuseWithoutProgram(m_recipe, "set", "parameters")) return;
  // A parameter struct with no fields lays out to nothing while still
  // occupying a byte as a C++ object, so its size can never be the
  // upload's; there is simply nothing to copy.
  if (*schema == m_recipe->parameters() && schema->fields.empty()) return;
  if (*schema != m_recipe->parameters() || size != m_bytes.size()) {
    reportOnce("parameters:" + m_recipe->name(),
               "recipe \"" + m_recipe->name() +
                   "\": the parameter struct given is not the one the recipe "
                   "was defined over; the values are ignored");
    return;
  }
  std::memcpy(m_bytes.data(), parameters, size);
}

const std::byte* Material::fieldBytes(std::string_view name,
                                      size_t floats) const {
  const Field* field = m_recipe ? m_recipe->parameters().find(name) : nullptr;
  if (!field || field->floats != floats) return nullptr;
  return m_bytes.data() + field->offset;
}

void Material::write(std::string_view name, ParameterType kind,
                     const void* floats, size_t count) {
  if (writePartInput(name, std::span<const float>(
                               static_cast<const float*>(floats), count)))
    return;
  if (refuseWithoutProgram(m_recipe, "set", name)) return;
  const Field* f = m_recipe->parameters().find(name);
  // THE REPORT'S KEY IS BUILT WHERE IT IS REPORTED. This is the per-field
  // setter, called once per field of every material built, and a string
  // assembled on the way past would be an allocation per field spent on
  // a message almost no call makes.
  const auto key = [&] {
    return "field:" + m_recipe->name() + ":" + std::string(name);
  };
  if (!f) {
    reportOnce(key(), "recipe \"" + m_recipe->name() +
                          "\" declares no field \"" + std::string(name) +
                          "\"; the value is ignored");
    return;
  }
  // A FIELD NO BODY READS is a dial that does nothing: the bytes go up
  // and the picture does not change, which at a call site reads exactly
  // like a wrong value. It is reported HERE rather than where a program
  // is compiled because here is where somebody wrote to it — a parameters
  // struct that carries a field this recipe's kind has no use for is a
  // shared ABI and not a mistake, and poured in whole it says nothing.
  if (!m_recipe->readsField(*f))
    reportOnce(key(), "recipe \"" + m_recipe->name() + "\" field \"" +
                          std::string(name) +
                          "\" is read by no body of it; writing to it has no "
                          "effect on the picture");
  // Colour and float4 interchange — both are four floats and the shader
  // declares one float4 — but a count mismatch means a different uniform.
  if (f->floats != count) {
    reportOnce(key(), "recipe \"" + m_recipe->name() + "\" field \"" +
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
                         motion::Animatable<float> value) {
  if (!m_recipe && m_composition && m_composition->source) {
    if (std::shared_ptr<const detail::Part> next =
            m_composition->source->withBinding(name, value)) {
      compose().source = std::move(next);
      return *this;
    }
  }
  if (refuseWithoutProgram(m_recipe, "bind", name)) return *this;
  const Field* f = m_recipe->parameters().find(name);
  if (!f || f->kind != ParameterType::Float) {
    reportOnce("bind:" + m_recipe->name() + ":" + std::string(name),
               "recipe \"" + m_recipe->name() + "\" has no float field \"" +
                   std::string(name) + "\" to bind a value to");
    return *this;
  }
  if (Binding* b = binding(name)) {
    b->value = std::move(value);
    b->block = nullptr;
    return *this;
  }
  m_bindings.push_back({std::string(name), std::move(value), nullptr});
  return *this;
}

Material& Material::bind(std::string_view name,
                         motion::Animatable<Color> value) {
  if (refuseWithoutProgram(m_recipe, "bind", name)) return *this;
  const Field* f = m_recipe->parameters().find(name);
  if (!f || f->kind != ParameterType::Color || f->floats != 4) {
    reportOnce("bind:" + m_recipe->name() + ":" + std::string(name),
               "recipe \"" + m_recipe->name() + "\" has no color field \"" +
                   std::string(name) + "\" to bind a value to");
    return *this;
  }
  if (Binding* b = binding(name)) {
    b->value = std::move(value);
    b->block = nullptr;
    return *this;
  }
  m_bindings.push_back({std::string(name), std::move(value), nullptr});
  return *this;
}

Material& Material::unbind(std::string_view name) {
  std::erase_if(m_bindings, [&](const Binding& x) { return x.name == name; });
  return *this;
}

Material& Material::bind(std::string_view name,
                         std::shared_ptr<const UniformBlock> block) {
  if (refuseWithoutProgram(m_recipe, "bind", name)) return *this;
  const Field* f = m_recipe->parameters().find(name);
  const std::string key = "bind:" + m_recipe->name() + ":" + std::string(name);
  if (!f || f->kind != ParameterType::FloatArray) {
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
    b->value = 0.0f;
    b->block = std::move(block);
    if (!b->block)
      std::erase_if(m_bindings,
                    [&](const Binding& x) { return x.name == name; });
    return *this;
  }
  if (block) m_bindings.push_back({std::string(name), 0.0f, std::move(block)});
  return *this;
}

bool Material::isBound(std::string_view name) const {
  for (const Binding& b : m_bindings)
    if (b.name == name) return true;
  return false;
}

void Material::place(std::string_view name, Slot slot) {
  if (refuseWithoutProgram(m_recipe, "slot", name)) return;
  const auto slots = m_recipe->slots();
  if (std::find(slots.begin(), slots.end(), name) == slots.end()) {
    reportOnce("child:" + m_recipe->name() + ":" + std::string(name),
               "recipe \"" + m_recipe->name() + "\" declares no slot \"" +
                   std::string(name) + "\"; the child is ignored");
    return;
  }
  for (auto& [existing, s] : m_slots) {
    if (existing == name) {
      s = std::move(slot);
      return;
    }
  }
  m_slots.emplace_back(std::string(name), std::move(slot));
  // Recipe order, so two materials filling the same slots in different
  // orders compare equal.
  std::sort(m_slots.begin(), m_slots.end(), [&](const auto& a, const auto& b) {
    return std::find(slots.begin(), slots.end(), a.first) <
           std::find(slots.begin(), slots.end(), b.first);
  });
}

Material& Material::slot(std::string_view name, Material material) {
  place(name, {std::make_shared<const Material>(std::move(material)), nullptr});
  return *this;
}

Material& Material::slot(std::string_view name,
                         std::shared_ptr<const Leaf> leaf) {
  place(name, {nullptr, std::move(leaf)});
  return *this;
}

const Material* Material::slot(std::string_view name) const {
  for (const auto& [slot, s] : m_slots)
    if (slot == name) return s.material.get();
  return nullptr;
}

const Leaf* Material::leaf(std::string_view name) const {
  for (const auto& [slot, s] : m_slots)
    if (slot == name) return s.leaf.get();
  return nullptr;
}

Material& Material::amount(float fraction) {
  m_amount = std::clamp(fraction, 0.0f, 1.0f);
  return *this;
}

Material& Material::quantizeTime(float rate) {
  m_quantizeHz = rate > 0.0f ? rate : 0.0f;
  return *this;
}

Material& Material::worldSpace(bool on) {
  m_worldSpace = on;
  return *this;
}

bool Material::isRunning() const {
  // A bound BLOCK is live by construction — the host revises it — and a
  // bound animatable is live exactly when SigilMotion says it is: a
  // plain number written into a uniform every resolve moves nothing.
  for (const Binding& b : m_bindings)
    if (b.block ||
        std::visit([](const auto& value) { return value.isRunning(); },
                   b.value))
      return true;
  if (m_recipe && m_recipe->reads(FrameInput::Time)) return true;
  if (m_composition) {
    const Composition& parts = *m_composition;
    if (parts.source && parts.source->isRunning()) return true;
    if (parts.effects && parts.effects->isRunning()) return true;
    if (parts.surface &&
        (surfaceChannelsMatch(
             *parts.surface,
             [](const Material& material) { return material.isRunning(); }) ||
         (parts.surface->lighting && parts.surface->lighting->isRunning())))
      return true;
    for (const Layer& layer : parts.layers)
      if (layer.source.isRunning() ||
          (layer.options.mask && layer.options.mask->source.isRunning()))
        return true;
  }
  for (const auto& [slot, s] : m_slots) {
    if (s.material && s.material->isRunning()) return true;
    if (s.leaf && s.leaf->animated()) return true;
  }
  return false;
}

bool Material::geometryDependent() const {
  if (m_worldSpace) return true;
  if (m_recipe && (m_recipe->reads(FrameInput::Resolution) ||
                   m_recipe->reads(FrameInput::WorldTransform) ||
                   m_recipe->reads(FrameInput::LocalToSample) ||
                   m_recipe->reads(FrameInput::ContentScale)))
    return true;
  if (m_composition) {
    const Composition& parts = *m_composition;
    if (parts.source && parts.source->geometryDependent()) return true;
    if (parts.effects && parts.effects->geometryDependent()) return true;
    if (parts.surface &&
        surfaceChannelsMatch(*parts.surface, [](const Material& material) {
          return material.geometryDependent();
        }))
      return true;
    for (const Layer& layer : parts.layers)
      if (layer.source.geometryDependent() ||
          (layer.options.mask &&
           layer.options.mask->source.geometryDependent()))
        return true;
  }
  for (const auto& [slot, s] : m_slots) {
    if (s.material && s.material->geometryDependent()) return true;
    if (s.leaf && s.leaf->geometryDependent()) return true;
  }
  return false;
}

bool Material::operator==(const Material& other) const {
  if (m_recipe != other.m_recipe || m_bytes != other.m_bytes ||
      m_amount != other.m_amount || m_quantizeHz != other.m_quantizeHz ||
      m_worldSpace != other.m_worldSpace ||
      m_bindings.size() != other.m_bindings.size() ||
      m_slots.size() != other.m_slots.size())
    return false;
  if (m_composition != other.m_composition) {
    if (!m_composition || !other.m_composition) return false;
    const Composition& a = *m_composition;
    const Composition& b = *other.m_composition;
    if (a.color != b.color || !samePart(a.source, b.source) ||
        !samePart(a.effects, b.effects) || a.surface != b.surface ||
        a.layers != b.layers)
      return false;
  }
  for (const Binding& a : m_bindings) {
    const Binding* b = nullptr;
    for (const Binding& x : other.m_bindings)
      if (x.name == a.name) b = &x;
    if (!b || a.value != b->value || a.block != b->block) return false;
  }
  for (size_t i = 0; i < m_slots.size(); ++i) {
    const auto& [slot, s] = m_slots[i];
    const auto& [otherSlot, o] = other.m_slots[i];
    if (slot != otherSlot) return false;
    if ((s.material != nullptr) != (o.material != nullptr)) return false;
    if (s.material && !(*s.material == *o.material)) return false;
    if ((s.leaf != nullptr) != (o.leaf != nullptr)) return false;
    if (s.leaf && !(*s.leaf == *o.leaf)) return false;
  }
  return true;
}

Material::Resolved Material::resolve(Target target, const FrameData& frame,
                                     Variant variant) const {
  if (!m_recipe) return {};
  ResolveState& state = *m_resolve;
  const std::lock_guard lock(state.mutex);
  std::vector<std::byte>& scratch = state.scratch;
  const Schema& layout = m_recipe->layout();
  scratch.assign(layout.byteSize, std::byte{0});
  if (!m_bytes.empty())
    std::memcpy(scratch.data(), m_bytes.data(), m_bytes.size());
  for (const Binding& b : m_bindings) {
    const Field* f = m_recipe->parameters().find(b.name);
    if (!f) continue;
    if (b.block) {
      const std::span<const float> values = b.block->committedValues();
      std::memcpy(scratch.data() + f->offset, values.data(),
                  values.size() * sizeof(float));
    } else {
      // No held motion: a material has no ticker, so what an animatable
      // reads here is its binding (shaped, where the chain shapes it) or
      // its plain number.
      std::visit(
          [&](const auto& source) {
            const auto value = source.value();
            std::memcpy(scratch.data() + f->offset, &value, sizeof(value));
          },
          b.value);
    }
  }
  const auto put = [&](FrameInput input, const void* floats, size_t count) {
    if (!m_recipe->reads(input)) return;
    const Field* f = layout.find(uniformName(input));
    std::memcpy(scratch.data() + f->offset, floats, count * sizeof(float));
  };
  const float seconds =
      motion::quantizeTime((float)frame.seconds, m_quantizeHz);
  put(FrameInput::Time, &seconds, 1);
  put(FrameInput::Resolution, &frame.resolution, 2);
  put(FrameInput::ContentScale, &frame.contentScale, 1);
  put(FrameInput::WorldTransform, &frame.world, 9);
  put(FrameInput::LocalToSample, &frame.localToSample, 9);

  for (size_t i = 0; i < state.uploads.size(); ++i) {
    ResolveState::Upload& upload = state.uploads[i];
    if (upload.valid && upload.target == target && upload.variant == variant &&
        *upload.bytes == scratch && upload.program) {
      state.latest = i;
      return {upload.program, upload.bytes};
    }
  }

  const auto compiled = program(m_recipe, target, variant);
  const size_t spare = 1 - state.latest;
  const size_t index =
      !state.uploads[spare].bytes || state.uploads[spare].bytes.use_count() == 1
          ? spare
          : state.latest;
  ResolveState::Upload& upload = state.uploads[index];
  if (!upload.bytes || upload.bytes.use_count() != 1)
    upload.bytes = std::make_shared<std::vector<std::byte>>(scratch);
  else
    upload.bytes->assign(scratch.begin(), scratch.end());
  upload.valid = true;
  upload.target = target;
  upload.variant = variant;
  upload.program = compiled;
  state.latest = index;
  return {upload.program, upload.bytes};
}

}  // namespace sigil::material
