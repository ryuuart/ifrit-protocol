/** @file
 * The harness's command line, spelled the way the gallery's headless
 * mode is: an output directory, an optional single study, and the
 * machine-readable registry list a byte-identity sweep reads.
 */

#include <sigilworld/testing/Study.h>

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <string>
#include <vector>

namespace sigil::world::testing {

namespace {

std::string lowered(std::string text) {
  std::transform(text.begin(), text.end(), text.begin(),
                 [](unsigned char c) { return (char)std::tolower(c); });
  return text;
}

}  // namespace

int runStudies(std::span<const Study> studies, int argc, char* argv[]) {
  std::string outDir = "world_studies_out";
  std::string only;
  bool headless = false;
  for (int i = 1; i < argc; ++i) {
    const std::string arg = argv[i];
    if (arg == "--headless") {
      headless = true;
      if (i + 1 < argc && argv[i + 1][0] != '-') outDir = argv[++i];
    } else if (arg == "--list-studies") {
      for (const Study& study : studies)
        std::printf("%s\n", study.name.c_str());
      return 0;
    } else if (arg == "--study" && i + 1 < argc) {
      only = argv[++i];
    } else {
      std::fprintf(stderr, "unknown argument \"%s\"\n", arg.c_str());
      return 1;
    }
  }
  if (!headless) {
    std::fprintf(stderr,
                 "usage: world_studies --headless <outdir> "
                 "[--study <name>] [--list-studies]\n");
    return 1;
  }

  std::vector<const Study*> selected;
  for (const Study& study : studies) {
    if (only.empty() ||
        lowered(study.name).find(lowered(only)) != std::string::npos)
      selected.push_back(&study);
  }
  if (selected.empty()) {
    std::fprintf(stderr, "no study matches \"%s\"; known studies:\n",
                 only.c_str());
    for (const Study& study : studies)
      std::fprintf(stderr, "  %s\n", study.name.c_str());
    return 1;
  }

  int failures = 0;
  for (const Study* study : selected) {
    const bool written = capture(*study, outDir);
    std::printf("%-24s %4dx%-4d  at %5.2fs  %s\n", study->name.c_str(),
                study->canvas.width(), study->canvas.height(),
                (double)study->captureSeconds, written ? "ok" : "FAILED");
    failures += written ? 0 : 1;
  }
  return failures == 0 ? 0 : 1;
}

}  // namespace sigil::world::testing
