/** @file
 * A Package loaded: the archive description opened with 8-bit-only
 * output options, one graph instance per graph, the renderer they
 * share, and a Graph handed out for each; torn down in the order the
 * framework's ownership requires.
 */

#include "sigilsubstance/package/Package.h"

#include <substance/framework/framework.h>

#include <fstream>
#include <iterator>
#include <vector>

#include "graph/GraphImpl.h"

namespace sigil::substance {

namespace air = SubstanceAir;

struct Package::Impl {
  std::unique_ptr<air::PackageDesc> desc;
  air::GraphInstances instances;
  std::unique_ptr<air::Renderer> renderer;
  std::vector<std::unique_ptr<Graph>> graphs;
};

Package::Package() : m_impl(std::make_unique<Impl>()) {}
Package::~Package() {
  // Graphs hold raw pointers into instances and the renderer: drop them
  // first, then the renderer, then the instances, then the description.
  m_impl->graphs.clear();
  m_impl->renderer.reset();
  m_impl->instances.clear();
  m_impl->desc.reset();
}

std::unique_ptr<Package> Package::load(const void* bytes, size_t size,
                                       std::string* error) {
  if (!bytes || size == 0) {
    if (error) *error = "empty archive";
    return nullptr;
  }
  std::unique_ptr<Package> package(new Package());
  Impl& impl = *package->m_impl;
  // Only 8-bit raw outputs, no mip pyramids: the two forms the graph
  // feature turns into SkImages. The engine substitutes these for
  // anything the graph authored otherwise.
  air::OutputOptions options;
  options.mAllowedFormats = air::Format_RGBA8 | air::Format_L8;
  options.mMipmap = air::Mipmap_ForceNone;
  impl.desc = std::make_unique<air::PackageDesc>(bytes, size, options);
  if (!impl.desc->isValid()) {
    if (error) *error = "not a valid Substance archive";
    return nullptr;
  }
  air::instantiate(impl.instances, *impl.desc);
  impl.renderer = std::make_unique<air::Renderer>();
  for (const auto& instance : impl.instances) {
    std::unique_ptr<Graph> graph(new Graph());
    graph->m_impl->instance = instance.get();
    graph->m_impl->renderer = impl.renderer.get();
    graph->m_impl->label = str(instance->mDesc.mLabel);
    graph->m_impl->url = str(instance->mDesc.mPackageUrl);
    impl.graphs.push_back(std::move(graph));
  }
  return package;
}

std::unique_ptr<Package> Package::load(const std::filesystem::path& file,
                                       std::string* error) {
  std::ifstream in(file, std::ios::binary);
  if (!in) {
    if (error) *error = "cannot read " + file.string();
    return nullptr;
  }
  std::vector<char> bytes((std::istreambuf_iterator<char>(in)),
                          std::istreambuf_iterator<char>());
  return load(bytes.data(), bytes.size(), error);
}

size_t Package::graphCount() const { return m_impl->graphs.size(); }
Graph& Package::graph(size_t index) { return *m_impl->graphs.at(index); }
const Graph& Package::graph(size_t index) const {
  return *m_impl->graphs.at(index);
}
Graph* Package::find(std::string_view labelOrUrl) {
  for (auto& g : m_impl->graphs)
    if (g->label() == labelOrUrl || g->url() == labelOrUrl) return g.get();
  return nullptr;
}

std::string Package::engineVersion() {
  air::Renderer probe;
  const SubstanceVersion v = probe.getCurrentVersion();
  return std::string(v.platformImplName ? v.platformImplName : "") + " " +
         std::to_string(v.versionMajor) + "." + std::to_string(v.versionMinor) +
         "." + std::to_string(v.versionPatch);
}

}  // namespace sigil::substance
