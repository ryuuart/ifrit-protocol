#pragma once

/** @file
 * IMAGES CARRIED INSIDE ANOTHER FILE'S BYTES, found by their own
 * signature rather than by the container's index.
 *
 * A great many authoring formats — an animation runtime's scene file, a
 * game archive, a document with its illustrations inlined — store whole
 * encoded images end to end inside one blob, and a reader that has no
 * parser for the container can still recover every one of them: an
 * encoded image says where it begins and where it ends, and the bytes
 * between two of them usually carry the name the author gave the one
 * that follows.
 *
 * This is a LAST RESORT and says so. It knows nothing about the
 * container, so it cannot tell an image the container references from
 * one it has abandoned, it cannot see an image the container stored
 * compressed, and the name it recovers is whatever printable run stood
 * closest before the signature — which is the author's asset name in
 * every format that writes one, and something else entirely in a format
 * that does not. Where a container has a parser, use the parser.
 */

#include <cstddef>
#include <span>
#include <string>
#include <vector>

namespace sigil::image {

/** ONE IMAGE FOUND INSIDE A LARGER FILE: the half-open byte range it
 *  occupies, and the name that stood before it.
 *
 *  The range is handed back rather than the decoded pixels because a
 *  caller usually wants only a few of the images a container holds, and
 *  decoding the rest to find out which is the whole cost of the read. */
struct EmbeddedImage {
  size_t offset = 0;
  size_t length = 0;
  /** The last printable-ASCII run of at least `minimumNameLength` bytes
   *  standing between the previous image's end and this one's start.
   *  Empty where there was none. */
  std::string name;
};

/** How the scan reads a blob. */
struct EmbeddedScan {
  /** How long a printable run has to be before it counts as a name. Short
   *  runs in a binary stream are usually a container's own tags and type
   *  codes rather than anything an author typed. */
  size_t minimumNameLength = 4;
  /** Stop after this many; 0 is every one. A caller that wants the first
   *  thumbnail out of a large archive pays for one image, not for all. */
  size_t limit = 0;
};

/** EVERY PNG EMBEDDED IN @p bytes, in file order.
 *
 *  Each is bounded exactly: the scan walks the chunk lengths from the
 *  signature to IEND, so the range is the whole encoded image and nothing
 *  after it. A truncated or malformed image ends the scan rather than
 *  yielding a range that runs off the end — the bytes after a broken
 *  chunk table cannot be trusted to be an image either. */
[[nodiscard]] std::vector<EmbeddedImage> embeddedPngs(
    std::span<const std::byte> bytes, const EmbeddedScan& how = {});

}  // namespace sigil::image
