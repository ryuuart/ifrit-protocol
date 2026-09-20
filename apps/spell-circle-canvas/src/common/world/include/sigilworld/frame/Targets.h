#pragma once

/** @file
 * @ingroup world-frame
 * The resources a frame's passes write and read: named surfaces, the
 * images they stood as at the end of the frame before, and the point
 * sets a compute pass cooks.
 *
 * A name is bound to a SURFACE SLOT rather than to a surface, so two
 * resources whose live ranges do not overlap can be handed one surface
 * and the passes never know. Nothing here decides which names share a
 * slot — the ordering does, and calls `bind()` with the answer.
 */

#include <include/core/SkImage.h>
#include <include/core/SkRefCnt.h>
#include <include/core/SkSize.h>
#include <include/core/SkSurface.h>
#include <sigilworld/element/Geometry.h>

#include <boost/container/map.hpp>
#include <boost/container/set.hpp>
#include <cstdint>
#include <functional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

class SkCanvas;

namespace sigil::world {

/** THE FRAME'S RESOURCES, by name. Raster-backed: this is what a
 *  machine with no device can honestly answer with. */
class Targets {
 public:
  Targets() = default;

  /** The size every surface here is made at. Setting a different one
   *  drops everything, including what `previous()` would have said. */
  void extent(SkISize size);
  /** The size every surface here is made at; empty until one is set. */
  [[nodiscard]] SkISize extent() const { return m_extent; }

  /** Point @p name at surface slot @p slot. A negative slot gives the
   *  name a surface no other name shares. Re-binding a name to another
   *  slot moves it; the surfaces themselves stay. */
  void bind(std::string name, int slot);
  /** Keep @p name's image at the end of the frame, so the frame after
   *  can read it through `previous()`. */
  void keep(std::string name);
  /** Forget every binding, so a frame whose passes changed does not
   *  inherit the last one's. Surfaces and kept images survive. */
  void unbind();

  /** The canvas of @p name's surface, made on the first ask. Null when
   *  the extent is empty. */
  SkCanvas* canvas(std::string_view name);
  /** @p name as it stands now. Null when nothing has painted it. */
  [[nodiscard]] sk_sp<SkImage> image(std::string_view name);
  /** @p name as it stood at the end of the frame before. Null on the
   *  first frame, and for a name no one asked to keep. */
  [[nodiscard]] sk_sp<SkImage> previous(std::string_view name) const;

  /** The point set @p name, made empty on the first ask. */
  geometry::mesh::Cloud* points(std::string_view name);
  /** The point set @p name as it stands; null when nothing made it. */
  [[nodiscard]] const geometry::mesh::Cloud* points(
      std::string_view name) const;

  /** @p cloud STAMPED with @p stamp, formed once per distinct pair and
   *  kept while both stand; null when there is nothing to stamp. The
   *  answer is held under `stampKey`, a fold over the two VALUES, so a
   *  still set is neither formed twice nor uploaded twice, and what a
   *  frame does not ask for is let go at the end of it. @p key receives
   *  the number the stamping is held under, for a tier that keys an
   *  upload by the same one. */
  const geometry::mesh::Mesh* stamped(const geometry::mesh::Cloud& cloud,
                                      const geometry::mesh::Mesh& stamp,
                                      uint64_t* key = nullptr);

  /** How many stamped meshes have been FORMED here, over the store's
   *  whole life. A frame drawing a set that has not moved must not move
   *  this number. */
  [[nodiscard]] uint64_t stampings() const { return m_stampings; }

  /** How many surfaces the names bound here needed. */
  [[nodiscard]] int surfaces() const;

  /** WHERE AN IMAGE COMES FROM when the frame's passes did not paint it
   *  here — an executor that performed them on a device. It answers for
   *  one name at a time, so only what something asks for costs the
   *  crossing back.
   *  @trap Installing one hands that executor last frame too: `previous`
   *  answers null and `endFrame` keeps nothing. */
  using ImageSource = std::function<sk_sp<SkImage>(std::string_view)>;
  /** Installs @p source, on the terms above. */
  void source(ImageSource source) { m_source = std::move(source); }
  /** Whether one is installed — which is also whether `previous()` and
   *  `endFrame()` have stopped answering for themselves. */
  [[nodiscard]] bool sourced() const { return (bool)m_source; }

  /** Close the frame: every kept name's image becomes what `previous()`
   *  answers. */
  void endFrame();

 private:
  SkISize m_extent{0, 0};
  /** The shared slots, in slot order, and which names sit in them. */
  std::vector<sk_sp<SkSurface>> m_shared;
  boost::container::map<std::string, int> m_slotOf;
  /** The names holding a surface of their own. */
  boost::container::map<std::string, sk_sp<SkSurface>> m_own;
  boost::container::map<std::string, sk_sp<SkImage>> m_previous;
  boost::container::set<std::string> m_kept;
  boost::container::map<std::string, geometry::mesh::Cloud> m_points;
  /** A formed stamping and the frame it was last asked for in. */
  struct Stamping {
    geometry::mesh::Mesh mesh;
    /** The pair it was formed from, kept so that a lookup landing on
     *  this number is confirmed by value rather than by the fold. */
    geometry::mesh::Cloud cloud;
    geometry::mesh::Mesh stamp;
    uint64_t used = 0;
  };
  boost::container::map<uint64_t, Stamping> m_stamped;
  uint64_t m_frame = 0;
  uint64_t m_stampings = 0;
  ImageSource m_source;

  /** The surface @p name sits in, made on the first ask. */
  SkSurface* surfaceOf(std::string_view name);
};

}  // namespace sigil::world
