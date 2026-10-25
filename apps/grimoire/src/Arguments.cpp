/** @file
 * Reading the command line into the value every lane is asked through.
 */

#include "Arguments.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <limits>
#include <print>
#include <string>

namespace {

bool isSketchPath(const std::string& text) {
  const auto extension = std::filesystem::path(text).extension();
  return extension == ".cpp" || extension == ".py";
}

}  // namespace

std::optional<Arguments> parseArguments(int argc, char* argv[]) {
  Arguments args;
  for (int i = 1; i < argc; ++i) {
    const std::string arg = argv[i];
    if (arg == "--headless") {
      args.headless = true;
      if (i + 1 < argc && argv[i + 1][0] != '-') {
        args.sweepOptions.outputDirectory = argv[++i];
        args.headlessDirectoryNamed = true;
      }
    } else if (arg == "--inspect" || arg.starts_with("--inspect=")) {
      if (args.inspectPort) {
        std::println(stderr, "--inspect can be supplied only once");
        return std::nullopt;
      }
      args.inspectPort = 0;
      if (arg.size() > 9) {
        const std::string port = arg.substr(10);
        char* end = nullptr;
        const unsigned long value = std::strtoul(port.c_str(), &end, 10);
        if (port.empty() || *end != '\0' || value > 65535) {
          std::println(stderr, "--inspect=PORT takes a port from 0 to 65535");
          return std::nullopt;
        }
        args.inspectPort = (uint16_t)value;
      }
    } else if (arg == "--repl") {
      args.repl = true;
    } else if (arg == "--list") {
      args.list = true;
    } else if (arg == "--catalog") {
      args.catalog = true;
    } else if (arg == "--workspace") {
      if (i + 1 >= argc || argv[i + 1][0] == '\0' || argv[i + 1][0] == '-' ||
          !args.workspace.empty()) {
        std::println(stderr, "--workspace requires one directory");
        return std::nullopt;
      }
      args.workspace = argv[++i];
    } else if (arg == "--no-restore" || arg == "--examples") {
      args.noRestore = true;
    } else if (arg == "--python-info") {
      args.pythonInfo = true;
    } else if (arg == "--python-executable" || arg == "--python-abi") {
      if (i + 1 >= argc || argv[i + 1][0] == '\0' || argv[i + 1][0] == '-') {
        std::println(stderr, "{} requires a value", arg);
        return std::nullopt;
      }
      if (arg == "--python-executable") {
        if (!args.pythonExecutable.empty()) {
          std::println(stderr, "--python-executable can be supplied only once");
          return std::nullopt;
        }
        args.pythonExecutable = argv[++i];
      } else {
        if (!args.pythonAbi.empty()) {
          std::println(stderr, "--python-abi can be supplied only once");
          return std::nullopt;
        }
        args.pythonAbi = argv[++i];
      }
    } else if (arg == "--compare" && i + 2 < argc) {
      args.compareOptions.first = argv[++i];
      args.compareOptions.second = argv[++i];
    } else if (arg == "--video" && i + 1 < argc) {
      args.storyOptions.outputPath = argv[++i];
    } else if (arg == "--video-frames" && i + 1 < argc) {
      args.storyOptions.framesPerSketch = std::max(1, std::stoi(argv[++i]));
    } else if (arg == "--video-size" && i + 1 < argc) {
      const std::string size = argv[++i];
      const size_t by = size.find('x');
      if (by == std::string::npos) {
        std::println(stderr, "--video-size wants WIDTHxHEIGHT");
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
      if (args.selected.empty())
        args.selected = argv[++i];
      else
        args.alsoSelected.push_back(argv[++i]);
    } else if (arg == "--ledger") {
      args.sweepOptions.ledger = true;
    } else if (arg == "--shot" && i + 1 < argc) {
      args.shotPath = argv[++i];
    } else if (arg == "--assets" && i + 1 < argc) {
      args.assetsOverride = argv[++i];
    } else if (arg.starts_with("--publish=")) {
      args.publishName = arg.substr(10);
      if (args.publishName.empty()) return std::nullopt;
      args.publish = true;
    } else if (arg == "--publish") {
      args.publish = true;
      // The name is optional, and the one positional argument this
      // parser takes is a sketch file: a following token that is
      // another flag, or the file itself, is not the name.
      if (i + 1 < argc) {
        const std::string next = argv[i + 1];
        if (!next.empty() && next[0] != '-' && !isSketchPath(next))
          args.publishName = argv[++i];
      }
    } else if (arg == "--thumbnails") {
      args.warmThumbnails = true;
    } else if (arg == "--state") {
      if (i + 1 >= argc || argv[i + 1][0] == '\0' || argv[i + 1][0] == '-' ||
          !args.stateDirectory.empty()) {
        std::println(stderr, "--state requires one directory");
        return std::nullopt;
      }
      args.stateDirectory = argv[++i];
    } else if (arg == "--thumbnail-budget" && i + 1 < argc) {
      args.thumbnailBudget = std::chrono::milliseconds(
          (long long)std::lround(std::strtod(argv[++i], nullptr) * 1000.0));
    } else if (arg == "--pipeline-names") {
      args.namePipelines = true;
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
        std::println(stderr, "--window-size wants WIDTHxHEIGHT");
        return std::nullopt;
      }
      args.windowBench.width = std::max(1, std::stoi(size.substr(0, by)));
      args.windowBench.height = std::max(1, std::stoi(size.substr(by + 1)));
    } else if (arg == "--window-scale" && i + 1 < argc) {
      args.windowBench.scale = std::stod(argv[++i]);
    } else if (arg == "--frame" && i + 1 < argc) {
      args.capture.outputPath = argv[++i];
    } else if (arg == "--at" && i + 1 < argc) {
      args.capture.at = std::stod(argv[++i]);
      if (!std::isfinite(args.capture.at) || args.capture.at < 0) {
        std::println(stderr, "--at requires a finite nonnegative time");
        return std::nullopt;
      }
    } else if (arg == "--scale" && i + 1 < argc) {
      args.capture.scale = std::stof(argv[++i]);
      if (!std::isfinite(args.capture.scale) || args.capture.scale <= 0) {
        std::println(stderr, "--scale requires a finite positive value");
        return std::nullopt;
      }
      args.sweepOptions.density = args.capture.scale;
    } else if (arg == "--frames" && i + 1 < argc) {
      args.capture.frames = std::max(1, std::stoi(argv[++i]));
    } else if (arg == "--fps" && i + 1 < argc) {
      args.capture.fps = std::stod(argv[++i]);
      if (!std::isfinite(args.capture.fps) || args.capture.fps <= 0 ||
          !std::isfinite(1.0 / args.capture.fps) ||
          args.capture.fps > std::numeric_limits<int>::max()) {
        std::println(stderr, "--fps requires a finite positive frame rate");
        return std::nullopt;
      }
      args.storyOptions.framesPerSecond =
          std::max(1, (int)std::lround(args.capture.fps));
    } else if (arg == "--deterministic") {
      args.clockPolicy = sigil::motion::ClockPolicy::Advance;
    } else if (arg == "--no-deterministic") {
      args.clockPolicy = sigil::motion::ClockPolicy::Wall;
    } else if (args.sketchFile.empty() && isSketchPath(arg)) {
      args.sketchFile = arg;
    } else {
      std::println(stderr, "unknown argument \"{}\"", arg);
      return std::nullopt;
    }
  }
  if (args.pythonInfo && argc != 2) {
    std::println(stderr, "--python-info must be used on its own");
    return std::nullopt;
  }
  // THE MONTAGE ENCODES THE COMPILED-IN REGISTRY, and a file opened by
  // path or a workspace is not in it: encoding the registry in its place
  // would write another sketch's frames under the file's name. The
  // combination is refused here, before any device or session opens.
  if (!args.storyOptions.outputPath.empty() &&
      (!args.sketchFile.empty() || !args.workspace.empty())) {
    std::println(stderr,
                 "--video encodes compiled-in sketches named with --sketch, "
                 "and a sketch file or --workspace is not one; photograph a "
                 "file with --frame and --frames instead");
    return std::nullopt;
  }
  // SEVERAL NAMED SKETCHES ARE A SEQUENCE ONLY THE WINDOW'S LANE PRESENTS,
  // one after another in one process; every other lane answers one
  // sketch, and a second name it would pass over is refused rather than
  // dropped.
  if (!args.alsoSelected.empty() && args.windowBench.seconds <= 0.0) {
    std::println(stderr,
                 "--sketch names one sketch, except under --window-bench, "
                 "which presents each named sketch in the order named");
    return std::nullopt;
  }
  if (args.pythonExecutable.empty() != args.pythonAbi.empty()) {
    std::println(
        stderr,
        "--python-executable and --python-abi must be supplied together");
    return std::nullopt;
  }
  return args;
}
