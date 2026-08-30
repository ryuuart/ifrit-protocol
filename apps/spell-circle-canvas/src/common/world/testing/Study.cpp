/** @file
 * Rendering a study to its declared moment, and writing the plate.
 */

#include <include/core/SkCanvas.h>
#include <include/core/SkImageInfo.h>
#include <include/core/SkStream.h>
#include <include/encode/SkPngEncoder.h>
#include <sigilmotion/clock/Ticker.h>
#include <sigilworld/scene/Scene.h>
#include <sigilworld/testing/Study.h>

#include <algorithm>
#include <cmath>
#include <filesystem>
#include <optional>

namespace sigil::world::testing {

namespace {
/** The one step every study is walked at. A plate is a function of the
 *  declared moment, so the step may not depend on anything a machine
 *  decides. */
constexpr double kStep = 1.0 / 60.0;
}  // namespace

SkBitmap render(const Study& study) {
  SkBitmap bitmap;
  bitmap.allocPixels(
      SkImageInfo::MakeN32Premul(study.canvas.width(), study.canvas.height()));
  SkCanvas canvas(bitmap);

  motion::Ticker ticker;
  Scene scene(ticker);
  const int frames =
      std::max(1, (int)std::lround((double)study.captureSeconds / kStep));
  for (int step = 1; step <= frames; ++step) {
    ticker.tick(kStep);
    // The plate's size and its viewpoint are the harness's to state: a
    // study says what it is of, not where it lands.
    Frame frame = study.describe((float)(step * kStep));
    frame.extent(study.canvas).camera(study.camera);
    scene.render(frame);
  }

  canvas.clear(study.background.toSkColor());
  const std::optional<Camera> declared = scene.camera();
  scene.draw(canvas, declared ? *declared : study.camera);
  return bitmap;
}

bool capture(const Study& study, const std::string& outDir) {
  std::filesystem::create_directories(outDir);
  const SkBitmap bitmap = render(study);
  const std::string path = outDir + "/study_" + study.name + ".png";
  SkFILEWStream stream(path.c_str());
  return stream.isValid() && SkPngEncoder::Encode(&stream, bitmap.pixmap(), {});
}

}  // namespace sigil::world::testing
