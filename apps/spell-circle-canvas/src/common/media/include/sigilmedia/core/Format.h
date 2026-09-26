#pragma once

/** @file
 * @ingroup media-core
 * `Format`: every format this library writes, stills and movies in one
 * list.
 */

namespace sigil::media {

/** THE FORMATS THIS LIBRARY WRITES: `media::encode` writes the four
 *  stills, `media::Encoder` the movie. */
enum class Format {
  Png,
  Jpeg,
  Webp,
  Exr,
  Mp4,
};

}  // namespace sigil::media
