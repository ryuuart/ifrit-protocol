#pragma once

/** @file
 * @ingroup media-core
 * `Metadata`: what a document says about itself before any frame is
 * decoded — an image's size, frames and layers, a video's duration,
 * rate and codec — in one value for both.
 */

#include <chrono>
#include <cstdint>
#include <string>
#include <vector>

namespace sigil::media {

/** WHAT A DOCUMENT SAYS ABOUT ITSELF WITHOUT A DECODE: read by
 *  `sigil::io::probe<media::Metadata>(hub, uri)` or `media::probe(bytes)`
 *  from an image or a video alike. A field the document leaves unstated
 *  stays at its default. */
struct Metadata {
  int width = 0;
  int height = 0;
  /** How many frames: 1 for a still, more for an animation, the count a
   *  video's container states (0 when it states none). */
  int64_t frames = 1;
  /** How long one play of the document lasts; zero for a still. */
  std::chrono::duration<double> duration{};
  /** Frames a second, as a video's stream states it; 0 for an image. */
  double frameRate = 0.0;
  /** How many times an animation plays as encoded, negative for
   *  forever. */
  int repetitions = -1;
  /** The format's short name: "png", "openexr", "svg", "mp4", … */
  std::string format;
  /** A video's codec and container, as FFmpeg names them. */
  std::string codec;
  std::string container;
  /** How many channels a pixel carries. */
  int channels = 4;
  /** An HDR or float source: EXR, float TIFF, … */
  bool floatingPoint = false;
  bool hasAlpha = false;
  bool hasAudio = false;
  /** An EXR's subimages and channel-prefix layers. */
  std::vector<std::string> layers;
  /** An EXR's channel names. */
  std::vector<std::string> channelNames;

  bool operator==(const Metadata&) const = default;
};

}  // namespace sigil::media
