// TextFlow demo — exercises the two acceptance scenarios end-to-end and
// reports per-frame layout times:
//
//   A. A mixed Latin/Japanese/Korean/Chinese paragraph with rich-text spans
//      flowing around animated shapes; the layout reflows every frame while
//      words are occasionally re-styled and re-written (the "Chrome rich
//      text" test).
//   B. A Knuth-Plass justified paragraph whose words update periodically
//      with minimal lag.
//   C. Freeform typography: text along a spiral path and along an arbitrary
//      set of slanted lines (the Pretext-style demos).
//
// Frames render to PNG under ./textflow_demo_out for visual inspection.
// Each scene lives in its own Scene*.cpp file in this directory; shared
// helpers (palette, timing, PNG output) live in DemoSupport.h/.cpp.
//
//   ./build/bin/Release/textflow_demo [frames]

#include "DemoScenes.h"

#include <textflow/FontContext.h>

#include <include/ports/SkFontMgr_mac_ct.h>

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <filesystem>

int main(int argc, char **argv) {
  const int frames = argc > 1 ? std::max(1, std::atoi(argv[1])) : 240;

  const std::filesystem::path outputDirectory = "textflow_demo_out";
  std::filesystem::create_directories(outputDirectory);

  textflow::FontContext fontContext(SkFontMgr_New_CoreText(nullptr));

  sceneExclusions(fontContext, frames, outputDirectory);
  sceneKnuthPlass(fontContext, frames, outputDirectory);
  sceneFreeform(fontContext, outputDirectory);
  sceneExtreme(fontContext, outputDirectory);
  sceneTypography(fontContext, outputDirectory);
  sceneBabel(fontContext, outputDirectory);
  sceneFeatures(fontContext, outputDirectory);
  sceneCjk(fontContext, outputDirectory);
  sceneShapes(fontContext, outputDirectory);
  scenePretext(fontContext, frames, outputDirectory);
  sceneFallback(fontContext, outputDirectory);
  sceneHyperScripts(fontContext, outputDirectory);

  std::printf("PNGs written to %s\n",
              std::filesystem::absolute(outputDirectory).string().c_str());
  return 0;
}
