// SigilWeave demo — headless PNG panels for the library-only surfaces the
// interactive gallery does not cover: extreme geometries, typographic
// options, script coverage, OpenType features, CJK/vertical, SkPath
// exclusions, CJK fallback, and the standalone-pass features panel.
// (Animated/interactive showcases — exclusions, Knuth-Plass, rain, ripple,
// loop, hyper-scripts, paint effects — live in WeaveGallery.)
//
// PNGs land under ./weave_demo_out for visual inspection. Each scene
// lives in its own Scene*.cpp file in this directory; shared helpers
// (palette, timing, PNG output) live in DemoSupport.h/.cpp.
//
//   ./build/bin/Release/weave_demo

#include "DemoScenes.h"

#include <sigilweave/FontContext.h>

#include <sigilweave/ports/SystemFontManager.h>

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <filesystem>

int main(int argc, char **argv) {
  static_cast<void>(argc);
  static_cast<void>(argv);

  const std::filesystem::path outputDirectory = "weave_demo_out";
  std::filesystem::create_directories(outputDirectory);

  sigil::weave::FontContext fontContext(sigil::weave::ports::systemFontManager());

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
