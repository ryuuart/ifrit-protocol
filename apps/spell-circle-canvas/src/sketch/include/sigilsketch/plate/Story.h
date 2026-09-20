#pragma once

/** @file
 * @ingroup sketch-plate
 *
 * A vertical MP4 montage over the sketch registry.
 */

#include <sigilvideo/Types.h>

#include <cstdint>
#include <string>

namespace sigil::weave {
class FontContext;
}

namespace sigil::sketch {

class Assets;

/** WHAT `story()` IS TOLD: which sketches to film, at what size and
 *  rate, and where to write the file.
 *
 *  The defaults are a phone-shaped montage of the whole registry — ten
 *  frames of each sketch at 30 per second, with a short hold either
 *  side of the run. A caller that wants one sketch, another shape or
 *  another bit rate states those fields and leaves the rest.
 *
 *  Only `outputPath` has no useful default, and an empty one writes
 *  nothing. */
struct StoryOptions {
  std::string outputPath;  ///< where the MP4 is written
  /// The index of the one sketch to film; negative films every one.
  int only = -1;
  /// Which kind of sketch to film ("canvas", "set"); empty films all.
  std::string kind;
  int width = 1080;              ///< the montage's pixel width
  int height = 1920;             ///< the montage's pixel height
  int framesPerSecond = 30;      ///< the encoded frame rate
  int framesPerSketch = 10;      ///< how long each sketch is held
  int introFrames = 18;          ///< the hold before the first sketch
  int outroFrames = 18;          ///< the hold after the last
  int64_t bitRate = 12'000'000;  ///< the encoder's target, in bits per second
  /// Whether the encoder may use the machine's video hardware.
  video::HardwarePreference hardware = video::HardwarePreference::Preferred;
};

/** Encodes every selected, available sketch into one MP4. */
int story(const StoryOptions& options, weave::FontContext& fonts,
          Assets& assets);

}  // namespace sigil::sketch
