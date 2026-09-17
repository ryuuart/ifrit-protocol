#pragma once

#include <cstdint>

namespace spellcircle {

/** Toolkit-free receiver defaults and render-target bounds. Hosts apply saved
 *  settings over these values and convert colors and fonts at their UI seam. */
struct ReceiverDefaults {
  static constexpr int port = 27015;
  static constexpr int minimumCanvasSize = 16;
  static constexpr int maximumCanvasSize = 8192;
  static constexpr int canvasWidth = 4000;
  static constexpr int canvasHeight = 4000;
  static constexpr float scale = 1.0f;
  static constexpr float strokeWidth = 4.0f;
  static constexpr float labelOffset = 0.0f;
  static constexpr float pointDistance = 40.0f;
  static constexpr float boxWidth = 360.0f;
  static constexpr float boxHeight = 140.0f;
  static constexpr float boxPadding = 16.0f;
  static constexpr float boxDistance = 40.0f;
  static constexpr int fontSize = 36;
  static constexpr int fontWeight = 700;
  static constexpr bool fontItalic = false;
  static constexpr const char* fontFamily = "";
  /** Packed color in 0xAARRGGBB order. */
  static constexpr std::uint32_t color = 0xffff0000u;
  static constexpr double targetFramesPerSecond = 60.0;
};

}  // namespace spellcircle
