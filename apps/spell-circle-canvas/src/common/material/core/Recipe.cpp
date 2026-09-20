/** @file
 * Recipe definition: bodies per target, the generated declarations, and
 * the upload layout that appends the frame inputs to the parameters.
 */

#include "sigilmaterial/core/Recipe.h"

#include "sigilmaterial/core/Program.h"  // reportOnce

namespace sigil::material {

namespace {

constexpr FrameInput kFrameInputs[] = {FrameInput::Time, FrameInput::Resolution,
                                       FrameInput::ContentScale,
                                       FrameInput::WorldTransform};

/** Whether @p body spells @p name as a WHOLE IDENTIFIER, so a `low`
 *  inside `lowEdge` is a different name. */
bool spells(const std::string& body, std::string_view name) {
  const auto part = [](char c) {
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
           (c >= '0' && c <= '9') || c == '_';
  };
  for (size_t at = body.find(name); at != std::string::npos;
       at = body.find(name, at + 1)) {
    if (at > 0 && part(body[at - 1])) continue;
    const size_t end = at + name.size();
    if (end < body.size() && part(body[end])) continue;
    return true;
  }
  return false;
}

Field frameField(FrameInput input) {
  switch (input) {
    case FrameInput::Time:
      return {"uTime", Kind::Float, 1, 0};
    case FrameInput::Resolution:
      return {"uResolution", Kind::Vec2, 2, 0};
    case FrameInput::ContentScale:
      return {"uContentScale", Kind::Float, 1, 0};
    case FrameInput::WorldTransform:
      return {"uWorld", Kind::Mat3, 9, 0};
  }
  return {"", Kind::Float, 1, 0};
}

}  // namespace

std::string_view uniformName(FrameInput input) {
  switch (input) {
    case FrameInput::Time:
      return "uTime";
    case FrameInput::Resolution:
      return "uResolution";
    case FrameInput::ContentScale:
      return "uContentScale";
    case FrameInput::WorldTransform:
      return "uWorld";
  }
  return "";
}

Recipe::Recipe(std::string name, const Schema& parameters)
    : m_name(std::move(name)), m_parameters(parameters) {
  relayout();
}

Recipe Recipe::of(std::string name, const Schema& parameters) {
  return Recipe(std::move(name), parameters);
}

Recipe& Recipe::body(Target target, std::string source) {
  m_bodies[target] = std::move(source);
  rescan();
  return *this;
}

Recipe& Recipe::slot(std::string slot) {
  for (const std::string& s : m_slots)
    if (s == slot) return *this;
  m_slots.push_back(std::move(slot));
  return *this;
}

Recipe& Recipe::slot(std::string name, LayerFilter filter,
                     std::string amountField) {
  this->slot(name);
  // THE AMOUNT IS A PARAMETER OF THIS RECIPE. A name the parameters do
  // not carry as one float reads as zero at the executor, which makes a
  // Gaussian of zero — the layer unfiltered — so the slot silently
  // becomes a second copy of the picture and nothing points at the
  // declaration that asked for it.
  const Field* amount = m_parameters.find(amountField);
  if (!amount || amount->floats != 1)
    reportOnce("layerslot.amount:" + m_name + ":" + name,
               "recipe \"" + m_name + "\" slot \"" + name +
                   "\" is filled from the layer through a filter whose "
                   "amount is field \"" +
                   amountField +
                   "\", which the parameters do not declare as one float; "
                   "the filter runs at zero");
  for (LayerSlot& declared : m_layerSlots)
    if (declared.name == name) {
      declared.filter = filter;
      declared.amountField = std::move(amountField);
      rescan();
      return *this;
    }
  m_layerSlots.push_back(
      LayerSlot{std::move(name), filter, std::move(amountField)});
  rescan();
  return *this;
}

Recipe& Recipe::channelwise(std::string slot) {
  m_channelwise = std::move(slot);
  return *this;
}

Recipe& Recipe::frame(FrameInput input) {
  m_frame |= (uint8_t)input;
  relayout();
  return *this;
}

void Recipe::relayout() {
  m_layout = m_parameters;
  for (FrameInput input : kFrameInputs) {
    if (!reads(input)) continue;
    Field f = frameField(input);
    f.offset = m_layout.byteSize;
    m_layout.byteSize += f.floats * sizeof(float);
    m_layout.fields.push_back(std::move(f));
  }
}

const std::string* Recipe::body(Target target) const {
  auto it = m_bodies.find(target);
  return it == m_bodies.end() ? nullptr : &it->second;
}

bool Recipe::readsField(std::string_view name) const {
  if (m_bodies.empty() || name.empty()) return true;
  const std::vector<Field>& fields = m_parameters.fields;
  for (size_t i = 0; i < fields.size() && i < m_read.size(); ++i)
    if (fields[i].name == name) return m_read[i] != 0;
  return spelled(name);
}

bool Recipe::readsField(const Field& field) const {
  const std::vector<Field>& fields = m_parameters.fields;
  if (m_bodies.empty()) return true;
  if (&field >= fields.data() && &field < fields.data() + fields.size()) {
    const size_t index = size_t(&field - fields.data());
    if (index < m_read.size()) return m_read[index] != 0;
  }
  return readsField(field.name);
}

void Recipe::rescan() {
  m_read.resize(m_parameters.fields.size());
  for (size_t i = 0; i < m_parameters.fields.size(); ++i) {
    m_read[i] = spelled(m_parameters.fields[i].name) ? 1 : 0;
    // A FIELD AN EXECUTOR READS IS READ. A layer slot's amount never
    // appears in a body — the filter it names is spent before the body
    // runs — and a field no body spells is otherwise reported as a dial
    // that does nothing the first time anybody writes to it.
    for (const LayerSlot& slot : m_layerSlots)
      if (slot.amountField == m_parameters.fields[i].name) m_read[i] = 1;
  }
}

bool Recipe::spelled(std::string_view name) const {
  for (const auto& [target, body] : m_bodies)
    if (spells(body, name)) return true;
  return false;
}

bool Recipe::samples(Target target, std::string_view slot) const {
  const std::string* b = body(target);
  if (!b || slot.empty()) return true;
  return spells(*b, slot);
}

std::vector<Target> Recipe::targets() const {
  std::vector<Target> out;
  out.reserve(m_bodies.size());
  for (const auto& [target, body] : m_bodies) out.push_back(target);
  return out;
}

std::string Recipe::declarations(Target target) const {
  std::string out = declare(m_layout, target);
  for (const std::string& slot : m_slots) {
    // A SLOT THIS TARGET'S BODY NEVER SAMPLES IS NOT DECLARED TO IT. A
    // declared slot is an image sampler in the compiled program whether
    // or not anything reads it, and a device has few — Metal binds
    // fragment textures at sixteen indices — so a stack composed for a
    // language handed one body per material would spend a program's
    // whole budget on the operand slots the language that samples its
    // operands does not use.
    if (!samples(target, slot)) continue;
    switch (target) {
      case Target::SkSL:
        out += "uniform shader " + slot + ";\n";
        break;
      case Target::Slang:
        out += "uniform Sampler2D " + slot + ";\n";
        break;
    }
  }
  return out;
}

std::string Recipe::source(Target target) const {
  const std::string* b = body(target);
  if (!b) return {};
  return declarations(target) + *b;
}

}  // namespace sigil::material
