#pragma once

/** @file
 * @ingroup material-skia
 *
 * THE FILTER'S SKIA EXECUTOR: what a `material::Filter` becomes when a
 * Skia canvas runs it — an `SkImageFilter` over the layer, or an
 * `SkColorFilter` for a per-pixel map — and the filters only Skia can hand
 * over: a raw image or colour filter, an SkSL program over the layer, and
 * a program lowered for the surface it will land on.
 */

#include <include/core/SkColorFilter.h>
#include <include/core/SkImageFilter.h>
#include <include/core/SkImageInfo.h>
#include <include/core/SkRefCnt.h>
#include <include/effects/SkRuntimeEffect.h>
#include <sigilmaterial/core/FrameData.h>
#include <sigilmaterial/filter/Filter.h>
#include <sigilmaterial/skia/Paint.h>

#include <span>
#include <string>
#include <utility>
#include <vector>

namespace sigil::material::skia {

/** @name Filters only Skia can supply
 *  @{ */
/** Any Skia image filter as a filter. It compares by pointer. */
Filter filter(sk_sp<SkImageFilter> imageFilter);
/** A Skia colour filter as a filter — a per-pixel map with no
 *  neighbourhood, which a consumer applies to the layer's paint rather
 *  than through the filter graph. It compares by pointer. */
Filter filter(sk_sp<SkColorFilter> colorFilter);
/** An SkSL runtime effect over the layer: the layer arrives in the slot
 *  named "content", and @p parameters are float uniforms set by name.
 *  @silent a name the effect does not declare as a float uniform (warned
 *  once). */
Filter program(sk_sp<SkRuntimeEffect> effect,
               std::vector<std::pair<std::string, float>> parameters = {});
/** THE SAME PROGRAM, LOWERED FOR THE SURFACE IT WILL LAND ON: a
 *  channelwise recipe over an eight-bit @p surface becomes a 256-entry
 *  table per channel and no program at all. Every other case is
 *  `Filter::of(program)`, so the picture is the same and only the cost
 *  differs. */
Filter lowered(const Material& program, SkColorType surface);
/** @} */

/** @name What a filter becomes
 *  @{ */
/** The image filter a static filter holds — null for none, and for a
 *  colour filter (ask `colorFilter`). */
sk_sp<SkImageFilter> imageFilter(const Filter& filter);
/** The colour filter, when the filter is a per-pixel map.
 *  @trap A filter answers one of the two, never both. */
sk_sp<SkColorFilter> colorFilter(const Filter& filter);
/** The filter with its bound parameters resolved NOW, as one image
 *  filter — a colour map lifted into the graph. @p frame is the painting
 *  node's, which slot sources resolve against; null is the context-free
 *  form. */
sk_sp<SkImageFilter> resolvedImageFilter(const Filter& filter,
                                         const FrameData* frame = nullptr);
/** EVERY SkSL BODY A STOCK FILTER IS BUILT OUT OF, as one list in a fixed
 *  order — the very objects the filters use, so a device backend can name
 *  one and rebuild its program at the next launch. Asking compiles all of
 *  them.
 *  @trap A body's PLACE in the list is part of its name, so the order is
 *  fixed and a body that would not compile is absent rather than null. */
std::span<const sk_sp<SkRuntimeEffect>> everyFilterProgram();
/** @} */

}  // namespace sigil::material::skia
