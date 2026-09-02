/** @file
 * The frame's named resources: surfaces made on first ask, the images
 * kept for the frame after, the point sets a compute pass cooks, and the
 * stamped meshes formed from them.
 */

#include <include/core/SkCanvas.h>
#include <include/core/SkColor.h>
#include <include/core/SkImageInfo.h>
#include <sigilworld/frame/Targets.h>

#include <iterator>
#include <string>
#include <utility>

namespace sigil::world {

namespace {

sk_sp<SkSurface> makeSurface(SkISize extent) {
  if (extent.isEmpty()) return nullptr;
  sk_sp<SkSurface> surface = SkSurfaces::Raster(
      SkImageInfo::MakeN32Premul(extent.width(), extent.height()));
  if (surface) surface->getCanvas()->clear(SK_ColorTRANSPARENT);
  return surface;
}

}  // namespace

void Targets::extent(SkISize size) {
  if (size == m_extent) return;
  m_extent = size;
  m_shared.clear();
  m_own.clear();
  m_previous.clear();
}

void Targets::bind(std::string name, int slot) {
  if (slot < 0) {
    m_slotOf.erase(name);
    m_own.try_emplace(std::move(name), nullptr);
    return;
  }
  m_own.erase(name);
  m_slotOf[std::move(name)] = slot;
  if ((int)m_shared.size() <= slot) m_shared.resize((size_t)slot + 1);
}

void Targets::keep(std::string name) { m_kept.insert(std::move(name)); }

void Targets::unbind() {
  m_slotOf.clear();
  // The names holding a surface of their own keep their surfaces: a
  // resource read back or read as a previous is the same resource from
  // one frame to the next, and dropping it here would lose the very
  // content the frame after asked for.
}

SkSurface* Targets::surfaceOf(std::string_view name) {
  const std::string key(name);
  if (const auto it = m_slotOf.find(key); it != m_slotOf.end()) {
    sk_sp<SkSurface>& surface = m_shared[(size_t)it->second];
    if (!surface) surface = makeSurface(m_extent);
    return surface.get();
  }
  const auto own = m_own.find(key);
  if (own == m_own.end()) return nullptr;
  if (!own->second) own->second = makeSurface(m_extent);
  return own->second.get();
}

SkCanvas* Targets::canvas(std::string_view name) {
  // A name no ordering bound is a name a pass asked for on its own; it
  // gets a surface of its own rather than nothing, so an escape-hatch
  // body can paint somewhere it did not declare.
  if (!surfaceOf(name)) bind(std::string(name), -1);
  SkSurface* surface = surfaceOf(name);
  return surface ? surface->getCanvas() : nullptr;
}

sk_sp<SkImage> Targets::image(std::string_view name) {
  // An installed source is where the pixels actually are, so it answers
  // first: with one, the surfaces here were never painted.
  if (m_source) return m_source(name);
  SkSurface* surface = surfaceOf(name);
  return surface ? surface->makeImageSnapshot() : nullptr;
}

sk_sp<SkImage> Targets::previous(std::string_view name) const {
  const auto it = m_previous.find(std::string(name));
  return it == m_previous.end() ? nullptr : it->second;
}

Cloud* Targets::points(std::string_view name) {
  return &m_points[std::string(name)];
}

const Cloud* Targets::points(std::string_view name) const {
  const auto it = m_points.find(std::string(name));
  return it == m_points.end() ? nullptr : &it->second;
}

const Mesh* Targets::stamped(const Cloud& cloud, const Mesh& stamp,
                             uint64_t* key) {
  if (cloud.positions.empty() || stamp.positions.empty()) return nullptr;
  const uint64_t folded = stampKey(cloud, stamp);
  if (key) *key = folded;
  const auto found = m_stamped.find(folded);
  if (found != m_stamped.end()) {
    found->second.used = m_frame;
    return &found->second.mesh;
  }
  Stamping made;
  // How the stamp rides its points is the point operators' own table —
  // one convention, so a cloud stands its stamps up the same way here
  // and through `pop::cookMesh`.
  made.mesh = cook(Stamped{cloud, stamp}).mesh;
  made.used = m_frame;
  ++m_stampings;
  return &m_stamped.emplace(folded, std::move(made)).first->second.mesh;
}

int Targets::surfaces() const {
  int count = 0;
  for (const sk_sp<SkSurface>& surface : m_shared)
    if (surface) ++count;
  for (const auto& [name, surface] : m_own)
    if (surface) ++count;
  return count;
}

void Targets::endFrame() {
  // A stamping no pass asked for this frame is let go: the set it came
  // from may have been dropped, and holding its mesh would hold a copy
  // of geometry nothing draws.
  for (auto it = m_stamped.begin(); it != m_stamped.end();)
    it = it->second.used < m_frame ? m_stamped.erase(it) : std::next(it);
  ++m_frame;
  // With a source installed, what "last frame" means belongs to the
  // executor that holds the pixels; keeping a raster copy here would
  // cost a crossing back for every kept name and answer with a second,
  // staler reading of the same resource.
  if (m_source) return;
  // A kept name a pass never painted answers with the transparent
  // surface it was bound to, which is the truthful reading of a
  // resource nothing has written yet.
  for (const std::string& name : m_kept)
    if (SkSurface* surface = surfaceOf(name))
      m_previous[name] = surface->makeImageSnapshot();
}

}  // namespace sigil::world
