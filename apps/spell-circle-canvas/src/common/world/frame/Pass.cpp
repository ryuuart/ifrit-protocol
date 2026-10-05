/** @file
 * The pass value: its setters, its factories, and the field-by-field
 * comparison a declaration is pruned by.
 */

#include <sigilmaterial/skia/Paint.h>
#include <sigilworld/advanced/Skia.h>
#include <sigilworld/frame/Pass.h>

#include <algorithm>
#include <utility>

namespace sigil::world {

namespace {

/** A body written as a lambda. It carries no `==`, so a pass holding one
 *  is equal to nothing but its own copies — which is the honest answer
 *  about a callable: two of them cannot be told apart. */
class LambdaBody : public PassBodyOperations {
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

Pass& Pass::only(Collection bodies) {
  m_collection = std::move(bodies);
  m_narrowed = true;
  return *this;
}

Pass& Pass::only(std::string name) {
  return only(Collection{.include = {std::move(name)}});
}

Pass& Pass::variant(::sigil::material::Material surface) {
  m_variant = std::move(surface);
  return *this;
}

Pass& Pass::realise(Selection realisation) {
  m_realisation = realisation;
  return *this;
}

Pass& Pass::clear(material::Color colour) {
  m_clear = colour;
  return *this;
}

Pass& Pass::chain(geometry::mesh::pop::Chain points, ChainOptions options) {
  m_chain = std::move(points);
  m_popRuntime = std::move(options.runtime);
  return *this;
}

Pass& Pass::stamp(geometry::mesh::Mesh body) {
  m_stamp = std::move(body);
  return *this;
}

Pass& Pass::blur(float sigma) {
  m_operation = Blur{sigma};
  return *this;
}

Pass& Pass::levels(Levels grade) {
  m_operation = grade;
  return *this;
}

Pass& Pass::composite(Composite layering) {
  m_operation = layering;
  return *this;
}

Pass& Pass::bloom(Bloom glow) {
  m_operation = glow;
  return *this;
}

Pass& Pass::body(PassBody b) {
  m_body = std::move(b);
  return *this;
}

Pass& Pass::body(core::Callable<void(const View&, Targets&)> fn) {
  m_body = PassBody(LambdaBody(std::move(fn)));
  return *this;
}

bool Pass::operator==(const Pass& other) const {
  return m_stage == other.m_stage && m_name == other.m_name &&
         m_reads == other.m_reads && m_writes == other.m_writes &&
         m_previous == other.m_previous && m_collection == other.m_collection &&
         m_narrowed == other.m_narrowed && m_variant == other.m_variant &&
         m_realisation == other.m_realisation && m_clear == other.m_clear &&
         m_chain == other.m_chain && m_popRuntime == other.m_popRuntime &&
         m_stamp == other.m_stamp && m_operation == other.m_operation &&
         m_body == other.m_body && m_producer == other.m_producer;
}

Pass geometryPass(std::string name) {
  return Pass(Stage::Geometry, std::move(name));
}

Pass computePass(std::string name) {
  return Pass(Stage::Compute, std::move(name));
}

Pass postPass(std::string name) { return Pass(Stage::Post, std::move(name)); }

Pass bloom(Bloom glow) {
  return postPass("bloom").reads("colour").writes("colour").bloom(glow);
}

Pass texturePass(std::string name, TextureProducer producer) {
  Pass pass(Stage::Texture, std::move(name));
  pass.m_producer = std::move(producer);
  return pass;
}

SkBlendMode toSk(material::BlendMode mode) {
  return material::skia::toSkBlendMode(mode);
}

}  // namespace sigil::world
