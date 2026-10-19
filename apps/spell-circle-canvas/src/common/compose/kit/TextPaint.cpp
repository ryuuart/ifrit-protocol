#include <sigilcompose/kit/TextPaint.h>
#include <sigilmaterial/program/Shader.h>
#include <sigilshaders/ComposeKit.h>

#include <algorithm>
#include <array>
#include <cstddef>
#include <string>
#include <string_view>

namespace sigil::compose::kit {
namespace {

/** The uniforms every text paint's program reads: where its box stands,
 *  how large it is, and the moment. The drift is read from the moment in
 *  the program, so a bound time moves it too. */
struct TextPaintParameters {
  glm::vec2 origin;
  glm::vec2 extent;
  float time;
};

/** One field's program file and the key its material answers to. */
struct FieldProgram {
  std::string_view file;
  std::string_view key;
};

constexpr std::array<FieldProgram, 6> kFieldPrograms{{
    {"TextPaint/Water.sksl", "textPaint.water"},
    {"TextPaint/MeshGradient.sksl", "textPaint.meshGradient"},
    {"TextPaint/Sparkle.sksl", "textPaint.sparkle"},
    {"TextPaint/Stars.sksl", "textPaint.stars"},
    {"TextPaint/Clouds.sksl", "textPaint.clouds"},
    {"TextPaint/Tunnel.sksl", "textPaint.tunnel"},
}};

/** The program of @p field: the shared drift ahead of the field's body,
 *  assembled once and held for the process. */
std::string_view programOf(TextField field) {
  static const std::array<std::string, kFieldPrograms.size()> programs = [] {
    std::array<std::string, kFieldPrograms.size()> assembled;
    const std::string_view drift = shaderSource("TextPaint/Drift.sksl");
    for (std::size_t index = 0; index < kFieldPrograms.size(); ++index)
      assembled[index] = std::string(drift) + "\n" +
                         std::string(shaderSource(kFieldPrograms[index].file));
    return assembled;
  }();
  return programs[static_cast<std::size_t>(field)];
}

}  // namespace

material::Material textPaint(const TextPaint& paint) {
  // A box with no area would divide by zero; it is held at a sliver.
  constexpr float kSliver = 1e-6f;
  const TextPaintParameters parameters{
      .origin = paint.box.min,
      .extent = {std::max(kSliver, paint.box.width()),
                 std::max(kSliver, paint.box.height())},
      .time = paint.time.value()};
  material::Material made = material::shader(
      programOf(paint.field), parameters,
      {.key = std::string(
           kFieldPrograms[static_cast<std::size_t>(paint.field)].key)});
  if (!paint.time.constant()) made.bind("time", paint.time);
  return made;
}

}  // namespace sigil::compose::kit
