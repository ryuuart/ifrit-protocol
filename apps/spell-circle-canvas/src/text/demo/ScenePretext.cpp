// Scene F — Pretext-style glyph choreography. Rain and ripple operate on a
// full ~1000-word paragraph that is re-laid-out EVERY frame (breathing
// measure, live word edits) while its letters simultaneously leave their
// lines as particles/waves; the loop marquee marches text around a closed
// contour forever. The point: letter-level choreography and full-paragraph
// relayout share one frame budget. See SceneRain.cpp, SceneRipple.cpp, and
// SceneLoop.cpp for each sub-scene.
#include "DemoScenes.h"

#include <cstdio>

void scenePretext(textflow::FontContext &fontContext, int frames,
                  const std::filesystem::path &outputDirectory) {
  std::printf("Scene F — Pretext-style glyph choreography (rain / pool "
              "ripple / infinite loop)\n");
  sceneRain(fontContext, frames, outputDirectory);
  sceneRipple(fontContext, frames, outputDirectory);
  sceneLoop(fontContext, frames, outputDirectory);
  std::printf("\n");
}
