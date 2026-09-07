/** @file
 * The pass value: its setters, its factories, and the field-by-field
 * comparison a declaration is pruned by.
 */

#include <sigilworld/frame/Pass.h>

#include <algorithm>
#include <utility>

namespace sigil::world {

namespace {

/** A body written as a lambda. It carries no `==`, so a pass holding one
 *  is equal to nothing but its own copies — which is the honest answer
 *  about a callable: two of them cannot be told apart. */
class LambdaBody : public PassBodyOps {
 public:
  explicit LambdaBody(std::function<void(const View&, Targets&)> fn)
      : m_fn(std::move(fn)) {}

  void run(const View& view, Targets& targets) const override {
    if (m_fn) m_fn(view, targets);
  }

 private:
  std::function<void(const View&, Targets&)> m_fn;
};

}  // namespace

Pass::Pass(Stage stage, std::string name)
    : m_stage(stage), m_name(std::move(name)) {}

void Pass::addRead(std::string name) {
  if (std::find(m_reads.begin(), m_reads.end(), name) == m_reads.end())
    m_reads.push_back(std::move(name));
}

void Pass::addWrite(std::string name) {
  if (std::find(m_writes.begin(), m_writes.end(), name) == m_writes.end())
    m_writes.push_back(std::move(name));
}

Pass& Pass::previous(std::string name) {
  if (std::find(m_previous.begin(), m_previous.end(), name) == m_previous.end())
    m_previous.push_back(std::move(name));
  return *this;
}

Pass& Pass::only(Selector selector) {
  m_selector = std::move(selector);
  m_narrowed = true;
  return *this;
}

Pass& Pass::variant(::sigil::material::Material surface) {
  m_variant = std::move(surface);
  return *this;
}

Pass& Pass::realise(Selection realisation) {
  m_realisation = realisation;
  return *this;
}

Pass& Pass::clear(SkColor4f colour) {
  m_clear = colour;
  return *this;
}

Pass& Pass::chain(geometry::mesh::pop::Chain c,
                  geometry::mesh::pop::Runtime runtime) {
  m_chain = std::move(c);
  m_popRuntime = std::move(runtime);
  return *this;
}

Pass& Pass::stamp(geometry::mesh::Mesh body) {
  m_stamp = std::move(body);
  return *this;
}

Pass& Pass::blur(float sigma) {
  m_op = Blur{sigma};
  return *this;
}

Pass& Pass::levels(float gain, float lift, SkColor4f tint) {
  m_op = Levels{gain, lift, tint};
  return *this;
}

Pass& Pass::composite(SkBlendMode mode, float opacity) {
  m_op = Composite{mode, opacity};
  return *this;
}

Pass& Pass::body(PassBody b) {
  m_body = std::move(b);
  return *this;
}

Pass& Pass::body(std::function<void(const View&, Targets&)> fn) {
  m_body = PassBody(LambdaBody(std::move(fn)));
  return *this;
}

bool Pass::operator==(const Pass& other) const {
  return m_stage == other.m_stage && m_name == other.m_name &&
         m_reads == other.m_reads && m_writes == other.m_writes &&
         m_previous == other.m_previous && m_selector == other.m_selector &&
         m_narrowed == other.m_narrowed && m_variant == other.m_variant &&
         m_realisation == other.m_realisation && m_clear == other.m_clear &&
         m_chain == other.m_chain && m_popRuntime == other.m_popRuntime &&
         m_stamp == other.m_stamp && m_op == other.m_op &&
         m_body == other.m_body;
}

Pass geometryPass(std::string name) {
  return Pass(Stage::Geometry, std::move(name));
}

Pass computePass(std::string name) {
  return Pass(Stage::Compute, std::move(name));
}

Pass postPass(std::string name) { return Pass(Stage::Post, std::move(name)); }

}  // namespace sigil::world
