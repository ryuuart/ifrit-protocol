#pragma once

/** @file A vertical MP4 montage over the sketch registry. */

#include <sigilvideo/Types.h>

#include <cstdint>
#include <string>

namespace sigil::weave {
class FontContext;
}

namespace sigil::sketch {

class Assets;

struct StoryOptions {
  std::string out;
  int only = -1;
  std::string kind;
  int width = 1080;
  int height = 1920;
  int framesPerSecond = 30;
  int framesPerSketch = 10;
  int introFrames = 18;
  int outroFrames = 18;
  int64_t bitRate = 12'000'000;
  video::HardwarePreference hardware = video::HardwarePreference::Preferred;
};

/** Encodes every selected, available sketch into one MP4. */
int story(const StoryOptions& options, weave::FontContext& fonts,
          Assets& assets);

}  // namespace sigil::sketch
