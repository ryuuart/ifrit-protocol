#pragma once

/** @file
 * @ingroup media-core
 * WRITING A PIXEL SOURCE: what a type must answer to be carried by a
 * `media::PixelSource` — a scene rendered to a texture, frames another
 * application publishes, a web view — and the members it may answer
 * besides. A caller holding sources never reads this; a library writing
 * one does.
 */

#include <include/core/SkSize.h>

#include <chrono>
#include <concepts>
#include <cstdint>

#include "sigilmedia/core/Frame.h"

namespace sigil::media {

/** WHAT A PIXEL SOURCE MUST BE: it answers the frame standing at a time,
 *  says whether that frame can change from one time to the next, and
 *  compares by value — two sources are equal when a consumer holding one
 *  may keep what it made of the other. */
template <class Source>
concept PixelSourceType =
    std::equality_comparable<Source> &&
    requires(const Source& source, std::chrono::duration<double> time) {
      { source.frameAt(time) } -> std::convertible_to<Frame>;
      { source.isRunning() } -> std::convertible_to<bool>;
    };

/** …and what it MAY answer besides, each optional, so a source that has
 *  no more to say is written with the two members above. `revision()`
 *  counts the frames that have stood — what a consumer compares to learn
 *  that a new one did; a source without it answers zero. */
template <class Source>
concept RevisedPixelSource = requires(const Source& source) {
  { source.revision() } -> std::convertible_to<uint64_t>;
};

/** `size()`, the frame size in pixels before a frame is asked for; a
 *  source without it answers the size of the frame at time zero. */
template <class Source>
concept SizedPixelSource = requires(const Source& source) {
  { source.size() } -> std::convertible_to<SkISize>;
};

/** A DOCUMENT read on a clock: an `Image` or a `Video`, anything whose
 *  frame at an elapsed time depends on a `Timing`. A `PixelSource` holds
 *  one shared with the timing it is read under. */
template <class Document>
concept TimedDocument = requires(const Document& document,
                                 std::chrono::duration<double> elapsed,
                                 const Timing& timing) {
  { document.frameAt(elapsed, timing) } -> std::convertible_to<Frame>;
  { document.isRunning() } -> std::convertible_to<bool>;
  { document.size() } -> std::convertible_to<SkISize>;
};

}  // namespace sigil::media
