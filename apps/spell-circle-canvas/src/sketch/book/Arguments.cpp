/** @file
 * Reading the command line into the value every lane is asked through.
 */

#include "Arguments.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <string>

namespace {

/** The default `--jitter-dt` amplitude: ±35% around the nominal
 *  interval, which is the spread a windowed host delivers when it is
 *  comfortably inside its budget and the compositor is merely uneven. */
constexpr double kDefaultJitter = 0.35;

}  // namespace

std::optional<Arguments> parseArguments(int argc, char* argv[]) {
  Arguments args;
  for (int i = 1; i < argc; ++i) {
    const std::string arg = argv[i];
    if (arg == "--headless") {
      args.headless = true;
      if (i + 1 < argc && argv[i + 1][0] != '-')
        args.sweepOptions.outDir = argv[++i];
    } else if (arg == "--list") {
      args.list = true;
    } else if (arg == "--catalog") {
      args.catalog = true;
    } else if (arg == "--compare" && i + 2 < argc) {
      args.compareOptions.first = argv[++i];
      args.compareOptions.second = argv[++i];
    } else if (arg == "--video" && i + 1 < argc) {
      args.storyOptions.out = argv[++i];
    } else if (arg == "--video-frames" && i + 1 < argc) {
      args.storyOptions.framesPerSketch = std::max(1, std::stoi(argv[++i]));
    } else if (arg == "--video-size" && i + 1 < argc) {
      const std::string size = argv[++i];
      const size_t by = size.find('x');
      if (by == std::string::npos) {
        std::fprintf(stderr, "--video-size wants WIDTHxHEIGHT\n");
        return std::nullopt;
      }
      args.storyOptions.width = std::max(2, std::stoi(size.substr(0, by)));
      args.storyOptions.height = std::max(2, std::stoi(size.substr(by + 1)));
    } else if (arg == "--video-bitrate" && i + 1 < argc) {
      args.storyOptions.bitRate = std::max<int64_t>(1, std::stoll(argv[++i]));
    } else if (arg == "--gpu") {
      args.gpu = true;
    } else if (arg == "--no-gpu") {
      args.noGpu = true;
    } else if (arg == "--kind" && i + 1 < argc) {
      args.kind = argv[++i];
    } else if (arg == "--sketch" && i + 1 < argc) {
      args.selected = argv[++i];
    } else if (arg == "--ledger") {
      args.sweepOptions.ledger = true;
    } else if (arg == "--no-promotion") {
      args.sweepOptions.noPromotion = true;
    } else if (arg == "--promotion") {
      args.sweepOptions.promotion = true;
    } else if (arg == "--capture-at" && i + 1 < argc) {
      args.sweepOptions.captureAt = std::strtod(argv[++i], nullptr);
    } else if (arg == "--timing-json" && i + 1 < argc) {
      args.sweepOptions.timingJson = argv[++i];
    } else if (arg == "--shot" && i + 1 < argc) {
      args.shotPath = argv[++i];
    } else if (arg == "--assets" && i + 1 < argc) {
      args.assetsOverride = argv[++i];
    } else if (arg == "--thumbnails") {
      args.warmThumbnails = true;
    } else if (arg == "--thumbnails-dir" && i + 1 < argc) {
      args.thumbnailDir = argv[++i];
    } else if (arg == "--thumbnail-budget" && i + 1 < argc) {
      args.thumbnailBudget = std::chrono::milliseconds(
          (long long)std::lround(std::strtod(argv[++i], nullptr) * 1000.0));
    } else if (arg == "--thumbnail-heavy") {
      args.thumbnailHeavy = true;
    } else if (arg == "--window-bench") {
      // The stretch is optional: a bare flag takes the default, and only
      // a following token that reads as a number is consumed.
      args.windowBench.seconds = kWindowBenchSeconds;
      if (i + 1 < argc) {
        const std::string next = argv[i + 1];
        if (!next.empty() &&
            (std::isdigit((unsigned char)next[0]) || next[0] == '.')) {
          args.windowBench.seconds = std::stod(next);
          ++i;
        }
      }
    } else if (arg == "--window-size" && i + 1 < argc) {
      const std::string size = argv[++i];
      const size_t by = size.find('x');
      if (by == std::string::npos) {
        std::fprintf(stderr, "--window-size wants WIDTHxHEIGHT\n");
        return std::nullopt;
      }
      args.windowBench.width = std::max(1, std::stoi(size.substr(0, by)));
      args.windowBench.height = std::max(1, std::stoi(size.substr(by + 1)));
    } else if (arg == "--window-scale" && i + 1 < argc) {
      args.windowBench.scale = std::stod(argv[++i]);
    } else if (arg == "--frame" && i + 1 < argc) {
      args.capture.out = argv[++i];
    } else if (arg == "--at" && i + 1 < argc) {
      args.capture.at = std::stod(argv[++i]);
    } else if (arg == "--scale" && i + 1 < argc) {
      args.capture.scale = std::stof(argv[++i]);
    } else if (arg == "--frames" && i + 1 < argc) {
      args.capture.frames = std::max(1, std::stoi(argv[++i]));
    } else if (arg == "--fps" && i + 1 < argc) {
      args.capture.fps = std::stod(argv[++i]);
      args.storyOptions.framesPerSecond =
          std::max(1, (int)std::lround(args.capture.fps));
    } else if (arg == "--bench") {
      args.capture.bench = true;
    } else if (arg == "--bench-frames" && i + 1 < argc) {
      args.capture.benchFrames = std::max(1, std::stoi(argv[++i]));
    } else if (arg == "--deterministic") {
      args.deterministic = true;
    } else if (arg == "--no-deterministic") {
      args.deterministic = false;
    } else if (arg == "--jitter-dt") {
      // The amplitude is optional: a bare flag takes the default, and
      // only a following token that reads as a number is consumed.
      args.capture.jitterDt = kDefaultJitter;
      if (i + 1 < argc) {
        const std::string next = argv[i + 1];
        if (!next.empty() &&
            (std::isdigit((unsigned char)next[0]) || next[0] == '.')) {
          args.capture.jitterDt = std::stod(next);
          ++i;
        }
      }
    } else if (args.sketchFile.empty() && arg.size() > 4 &&
               arg.compare(arg.size() - 4, 4, ".cpp") == 0) {
      args.sketchFile = arg;
    } else {
      std::fprintf(stderr, "unknown argument \"%s\"\n", arg.c_str());
      return std::nullopt;
    }
  }
  return args;
}
