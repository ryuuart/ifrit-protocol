#pragma once

/** @file
 * @ingroup compose-web
 *
 * SigilCompose × SigilScry — the web leaf. The compose kernel never
 * links SigilScry; include this only in targets that link
 * SigilComposeWeb.
 *
 * web(view) is definitional sugar, not new machinery: a Cache::None
 * custom leaf whose program draws the view's latest published frame
 * into the laid-out box. WebView::draw() absorbs the engine mode — on
 * GPU engines it wraps the frame texture zero-copy through the
 * canvas's Graphite recorder; on CPU engines it draws the raster
 * frame. Redraw pacing stays the host's event-driven contract: combine
 * WebView::setFrameCallback() (or frameVersion() polling) with your
 * needs-frame decision; the leaf always paints the newest frame when
 * the composer draws.
 *
 * The other direction — a Composer drawn INTO a page — needs no
 * adapter at all: draw into a WebImage::paint() callback's canvas.
 */

#include <sigilmaterial/texture/Texture.h>

#include <memory>

#include "sigilcompose/Compose.h"

namespace sigil::scry {
class WebView;
}

namespace sigil::compose {

/** A live web page as a leaf. Size it like any box; the frame scales
 *  into the laid-out bounds (letterboxing is the page's business —
 *  match the view size to the layout for 1:1 pixels). @p sampling is how
 *  the frame's pixels are read where it is scaled. */
Element web(std::shared_ptr<sigil::scry::WebView> view,
            material::Sampling sampling = material::Sampling::Linear);

}  // namespace sigil::compose
