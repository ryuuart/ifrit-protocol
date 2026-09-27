/** @file
 * The web leaf's drawing: the view's newest frame into the laid-out box,
 * through the pen's canvas, which is where a web view draws.
 */

#include "sigilcompose/web/Web.h"

#include <include/core/SkCanvas.h>
#include <include/core/SkSamplingOptions.h>
#include <sigildraw/Pen.h>
#include <sigilmaterial/skia/Texture.h>
#include <sigilscry/engine/WebView.h>

namespace sigil::compose {

Element web(std::shared_ptr<sigil::scry::WebView> view,
            material::Sampling sampling) {
  const SkSamplingOptions options(material::skia::toSkFilterMode(sampling));
  Element leaf = custom([view = std::move(view), options](
                            sigil::draw::Pen& pen, const PaintContext& ctx) {
    SkCanvas& canvas = *pen.canvas();
    if (view)
      view->draw(canvas, SkRect::MakeWH(ctx.size.x, ctx.size.y), options);
  });
  leaf.cache(Cache::None);  // live frames — declared volatility
  return leaf;
}

}  // namespace sigil::compose
