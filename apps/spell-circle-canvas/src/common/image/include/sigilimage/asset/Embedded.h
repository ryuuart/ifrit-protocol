#pragma once

/** @file
 * @ingroup image-asset
 * IMAGES CARRIED INSIDE ANOTHER FILE'S BYTES, found by their own
 * signature rather than by the container's index: an encoded image says
 * where it begins and where it ends, and the bytes between two of them
 * usually carry the name the author gave the one that follows.
 *
 * This is a LAST RESORT. It knows nothing about the container, so where
 * one has a parser, use the parser.
 */

#include <cstddef>
#include <span>
#include <string>
#include <vector>

namespace sigil::image {

/** ONE IMAGE FOUND INSIDE A LARGER FILE: the half-open byte range it
 *  occupies, and the name that stood before it. A range rather than the
 *  decoded pixels, because a caller usually wants only a few of the
 *  images a container holds. */
struct EmbeddedImage {
  size_t offset = 0;  ///< Where the encoded image begins in the blob.
  size_t length = 0;  ///< How many bytes of the blob it occupies.
  /** The last printable-ASCII run of at least `minimumNameLength` bytes
   *  standing between the previous image's end and this one's start;
   *  empty where there was none.
   *  @trap It is the author's asset name in every format that writes
   *  one, and something else entirely in a format that does not. */
  std::string name;
};

/** How the scan reads a blob. */
struct EmbeddedScan {
  /** How long a printable run has to be, in bytes, before it counts as
   *  a name. Shorter runs in a binary stream are usually a container's
   *  own tags and type codes. */
  size_t minimumNameLength = 4;
  /** Stop after this many; 0, the default, is every one. */
  size_t limit = 0;
};

/** EVERY PNG EMBEDDED IN @p bytes, in file order, each bounded exactly:
 *  the scan walks the chunk lengths from the signature to IEND, so a
 *  range is the whole encoded image and nothing after it.
 *  @trap A truncated or malformed image ENDS the scan rather than
 *  yielding a range that runs off the end. */
[[nodiscard]] std::vector<EmbeddedImage> embeddedPngs(
    std::span<const std::byte> bytes, const EmbeddedScan& how = {});

}  // namespace sigil::image
