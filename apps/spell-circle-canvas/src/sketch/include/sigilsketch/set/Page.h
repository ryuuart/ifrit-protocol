#pragma once

/** @file
 * @ingroup sketch-set
 *
 * A PAGE THE SET LIGHTS: a compose tree painted as the maps of a surface
 * by a pass on the frame, and the one material a body wears to show it.
 */

#include <sigilmaterial/core/Material.h>
#include <sigilworld/frame/Pass.h>

#include <memory>
#include <string>

namespace sigil::compose {
class Element;
class SurfaceScene;
}  // namespace sigil::compose

namespace sigil::sketch {

/** ONE PAGE, LIT BY THE SET: a compose tree painted as the maps of a
 *  surface, and the material a body wears to show it under the set's own
 *  lights, environment and camera.
 *
 *  Nothing paints unless its pass is on the frame:
 *
 *      return world::Frame(world::group({…,
 * world::mesh(plate).fill(page.surface())})) .pass(page.render(tree));
 *
 *  `render` is that pass, run at the frame's time before anything draws a
 *  body, and a body wearing `surface()` on a frame without it is an error
 *  naming both. A page whose tree has not changed paints nothing, and the
 *  frame counts it as skipped.
 *
 *  Made by `SetContext::page`, which keeps what it paints into for as long
 *  as the session runs. A copy is the same page. */
class Page {
 public:
  /** No page: its pass paints nothing and its surface is the stock one. */
  Page() = default;

  /** THE PASS THAT PAINTS @p tree as this page's maps, at the frame's
   *  time. Put it on the frame whose bodies wear `surface()`. */
  [[nodiscard]] world::Pass render(const compose::Element& tree) const;

  /** THE ONE MATERIAL a body wears to show this page, lit by the set. It
   *  is the extension point for what a page can say: what it is built from
   *  is this page's business, and nothing that wears it depends on that.
   *  The maps of an optional lobe — a sheen, a brushing, a film, a
   *  specular — are part of it once the page's content states the lobe;
   *  the page learns that while it paints, after the frame was described,
   *  and a body wearing this is drawn with them from that same frame. */
  [[nodiscard]] material::Material surface() const;

  /** The name the page's pass goes by, and that an error names. */
  [[nodiscard]] const std::string& name() const;

  /** @private what `SetContext::page` makes one from. */
  Page(std::shared_ptr<compose::SurfaceScene> scene, std::string name);

 private:
  std::shared_ptr<compose::SurfaceScene> m_scene;
  std::string m_name;
};

}  // namespace sigil::sketch
