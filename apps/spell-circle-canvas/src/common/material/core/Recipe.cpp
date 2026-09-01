/** @file
 * Recipe definition: bodies per target, the generated declarations, and
 * the upload layout that appends the frame inputs to the params.
 */

#include "sigilmaterial/core/Recipe.h"

namespace sigil::material {

namespace {

constexpr FrameInput kFrameInputs[] = {FrameInput::Time, FrameInput::Resolution,
                                       FrameInput::ContentScale,
                                       FrameInput::WorldTransform};

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

Recipe::Recipe(std::string name, const Schema& params)
    : m_name(std::move(name)), m_params(params) {
  relayout();
}

Recipe Recipe::of(std::string name, const Schema& params) {
  return Recipe(std::move(name), params);
}

Recipe& Recipe::body(Target target, std::string source) {
  m_bodies[target] = std::move(source);
  return *this;
}

Recipe& Recipe::child(std::string slot) {
  for (const std::string& s : m_children)
    if (s == slot) return *this;
  m_children.push_back(std::move(slot));
  return *this;
}

Recipe& Recipe::frame(FrameInput input) {
  m_frame |= (uint8_t)input;
  relayout();
  return *this;
}

void Recipe::relayout() {
  m_layout = m_params;
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

std::vector<Target> Recipe::targets() const {
  std::vector<Target> out;
  out.reserve(m_bodies.size());
  for (const auto& [target, body] : m_bodies) out.push_back(target);
  return out;
}

std::string Recipe::declarations(Target target) const {
  std::string out = declare(m_layout, target);
  for (const std::string& slot : m_children) {
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
