// TextFlow demo — headless PNG panels for the library-only surfaces the
// interactive gallery does not cover: extreme geometries, typographic
// options, script coverage, OpenType features, CJK/vertical, SkPath
// exclusions, CJK fallback, and the standalone-pass features panel.
// (Animated/interactive showcases — exclusions, Knuth-Plass, rain, ripple,
// loop, hyper-scripts, paint effects — live in TextFlowGallery.)
//
// PNGs land under ./textflow_demo_out for visual inspection. Each scene
// lives in its own Scene*.cpp file in this directory; shared helpers
// (palette, timing, PNG output) live in DemoSupport.h/.cpp.
//
//   ./build/bin/Release/textflow_demo

#include "DemoScenes.h"

#include <textflow/FontContext.h>

#include <textflow/ports/SystemFontManager.h>

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <filesystem>

int main(int argc, char **argv) {
  static_cast<void>(argc);
  static_cast<void>(argv);

  const std::filesystem::path outputDirectory = "textflow_demo_out";
  std::filesystem::create_directories(outputDirectory);

  textflow::FontContext fontContext(textflow::ports::systemFontManager());

  sceneExtreme(fontContext, outputDirectory);
  sceneTypography(fontContext, outputDirectory);
  sceneBabel(fontContext, outputDirectory);
  sceneFeatures(fontContext, outputDirectory);
  sceneCjk(fontContext, outputDirectory);
  sceneShapes(fontContext, outputDirectory);
  sceneFallback(fontContext, outputDirectory);
  sceneNewFeatures(fontContext, outputDirectory);

  std::printf("PNGs written to %s\n",
              std::filesystem::absolute(outputDirectory).string().c_str());
  return 0;
}
