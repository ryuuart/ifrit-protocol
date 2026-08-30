#pragma once

/** @file
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

#include <map>
#include <set>
#include <string>
#include <string_view>
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
  Cloud* points(std::string_view name);
  [[nodiscard]] const Cloud* points(std::string_view name) const;

  /** How many surfaces the names bound here needed. */
  [[nodiscard]] int surfaces() const;

  /** Close the frame: every kept name's image becomes what `previous()`
   *  answers. */
  void endFrame();

 private:
  SkISize m_extent{0, 0};
  /** The shared slots, in slot order, and which names sit in them. */
  std::vector<sk_sp<SkSurface>> m_shared;
  std::map<std::string, int> m_slotOf;
  /** The names holding a surface of their own. */
  std::map<std::string, sk_sp<SkSurface>> m_own;
  std::map<std::string, sk_sp<SkImage>> m_previous;
  std::set<std::string> m_kept;
  std::map<std::string, Cloud> m_points;

  /** The surface @p name sits in, made on the first ask. */
  SkSurface* surfaceOf(std::string_view name);
};

}  // namespace sigil::world
