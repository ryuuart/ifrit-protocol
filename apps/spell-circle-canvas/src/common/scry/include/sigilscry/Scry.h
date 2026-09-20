#pragma once

/** @file
 * SigilScry — a headless web engine whose HTML, CSS and JavaScript
 * output arrives as Skia images. The umbrella header: the engine, its
 * views and its image slots, for a consumer of the whole library.
 * Namespace sigil::scry, target SigilScry.
 */

/** @defgroup scry-engine The engine
 *  The headless browser itself: the engine, the views loaded into it,
 *  and the image each view's pixels arrive as.
 *  @{ */
/** @} */

/** @defgroup scry-platform The platform seam
 *  What the engine asks of its host — files, logging, surfaces — and
 *  the levels a log line carries.
 *  @{ */
/** @} */

#include <sigilscry/engine/WebEngine.h>
#include <sigilscry/engine/WebImage.h>
#include <sigilscry/engine/WebView.h>
#include <sigilscry/platform/LogLevel.h>
