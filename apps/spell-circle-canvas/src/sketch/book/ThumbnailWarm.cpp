/** @file
 * Where the app keeps its thumbnails, and the warm command that renders
 * the ones nothing has drawn yet.
 */

#include "ThumbnailWarm.h"

#include <sigilsketch/core/Assets.h>
#include <sigilsketch/core/Crash.h>
#include <sigilsketch/core/Registry.h>
#include <sigilsketch/core/Sources.h>
#include <sigilsketch/plate/Thumbnails.h>

#include <QtCore/QStandardPaths>
#include <QtCore/QString>
#include <cstdio>
#include <cstdlib>
#include <vector>

#include "SketchCatalog.h"

namespace sketch = sigil::sketch;

std::filesystem::path thumbnailStoreDirectory(const std::string& override) {
  if (!override.empty()) return override;
  if (const char* env = std::getenv("SIGIL_SKETCHBOOK_THUMBNAILS"); env && *env)
    return env;
  const QString cache =
      QStandardPaths::writableLocation(QStandardPaths::CacheLocation);
  const std::filesystem::path base =
      cache.isEmpty() ? std::filesystem::temp_directory_path()
                      : std::filesystem::path(cache.toStdString());
  return base / "Sketchbook" / "thumbnails";
}

int runThumbnails(int only, const std::string& kind,
                  const std::filesystem::path& directory,
                  std::chrono::milliseconds budget, bool heavy,
                  sigil::weave::FontContext& fonts, sketch::Assets& store) {
  std::filesystem::create_directories(directory);
  const auto& entries = sketch::registry();
  int rendered = 0;
  size_t skipped = 0;
  size_t noted = 0;
  std::vector<std::string> failed;
  for (int index : sketch::selection(only, kind)) {
    const sketch::Entry& entry = entries[index];
    std::string why;
    if (!entry.available(&why)) {
      std::printf("thumbnail %-24s [skipped: %s]\n", entry.name, why.c_str());
      ++skipped;
      continue;
    }
    const std::filesystem::path source =
        sketch::sourceOf(SketchCatalog::sketchDirectory, entry.key);
    const std::string key = sketch::thumbnailKey(source);
    if (!sketch::freshThumbnail(directory, entry.name, key).empty())
      continue;  // fresh
    if (!sketch::thumbnailNote(directory, entry.name, key).empty()) {
      ++noted;
      continue;  // asked and answered
    }
    sketch::ThumbnailRun run;
    run.outputPath = sketch::thumbnailFile(directory, entry.name, key);
    run.stem = entry.name;
    run.maxDimension = sketch::kThumbnailWidth;
    run.budget = budget;
    run.heavy = heavy;
    sketch::noteSketch(entry.name);
    switch (sketch::renderThumbnail(entry, fonts, store, run)) {
      case sketch::ThumbnailOutcome::Wrote:
        std::printf("thumbnail %-24s wrote %s\n", entry.name,
                    run.outputPath.string().c_str());
        ++rendered;
        break;
      case sketch::ThumbnailOutcome::Heavy:
        sketch::noteThumbnail(directory, entry.name, key, "declared a plate");
        std::printf("thumbnail %-24s [noted: declared a plate]\n", entry.name);
        ++noted;
        break;
      case sketch::ThumbnailOutcome::OverBudget:
        sketch::noteThumbnail(directory, entry.name, key,
                              "still ran past its budget");
        std::printf("thumbnail %-24s [noted: ran past its budget]\n",
                    entry.name);
        ++noted;
        break;
      case sketch::ThumbnailOutcome::Stopped:
      case sketch::ThumbnailOutcome::Failed:
        std::fprintf(stderr, "thumbnail %-24s FAILED to render\n", entry.name);
        failed.push_back(entry.name);
        break;
    }
  }
  std::printf("thumbnails: %d rendered, %zu noted, %zu skipped, %zu failed\n",
              rendered, noted, skipped, failed.size());
  for (const std::string& name : failed)
    std::fprintf(stderr, "  failed: %s\n", name.c_str());
  std::fflush(stdout);
  return failed.empty() ? 0 : 1;
}
