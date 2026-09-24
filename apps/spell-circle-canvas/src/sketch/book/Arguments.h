#pragma once

/** @file
 * The command line as one value: every flag Sketchbook takes, read once
 * into what the lane behind it is asked through.
 */

#include <sigilmotion/clock/ClockPolicy.h>
#include <sigilsketch/plate/Compare.h>
#include <sigilsketch/plate/Story.h>
#include <sigilsketch/plate/Sweep.h>
#include <sigilsketch/plate/Thumbnails.h>

#include <chrono>
#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>

#include "FrameLane.h"
#include "WindowBench.h"

/** WHAT THIS RUN WAS ASKED FOR. One flag names the lane — a comparison,
 *  a listing, the browser's rows, a sweep, a montage, the warm command,
 *  a still, a measurement — and the rest narrow it: which sketches, on
 *  which tier, where the pictures land, and whether the window offers
 *  what it draws to other applications. */
struct Arguments {
  std::filesystem::path sketchFile;
  std::filesystem::path workspace;
  std::filesystem::path assetsOverride;
  /** A launcher chooses one Python environment before any sketch is probed.
   *  The executable and its native extension ABI must be supplied together. */
  std::filesystem::path pythonExecutable;
  std::string pythonAbi;
  std::string selected, kind, shotPath;
  sigil::sketch::CompareOptions compareOptions;
  sigil::sketch::SweepOptions sweepOptions;
  sigil::sketch::StoryOptions storyOptions;
  CaptureOptions capture;
  WindowBench windowBench;
  bool headless = false, list = false, catalog = false, gpu = false;
  /** Whether `--headless` named the directory its plates go into. */
  bool headlessDirectoryNamed = false;
  bool noGpu = false;
  bool pythonInfo = false;
  bool noRestore = false;
  bool warmThumbnails = false;
  /** Whether the window offers its frames to other applications, and
   *  the name they are offered under — empty for the sketch's stem. */
  bool publish = false;
  std::string publishName;
  bool thumbnailHeavy = false;
  std::chrono::milliseconds thumbnailBudget = sigil::sketch::kThumbnailBudget;
  /** Where this run keeps everything it writes for a later run — builds,
   *  thumbnails, recorded device programs, settings and recents — each
   *  kind in a directory of its own. Empty leaves each where the platform
   *  keeps it. */
  std::filesystem::path stateDirectory;
  /** WHO MOVES THE CLOCK a still's session is drawn at: `--deterministic`
   *  is a client's steps (Advance), `--no-deterministic` the wall's.
   *  Unstated, a still is taken under Advance and a measurement under the
   *  wall's clock. */
  std::optional<sigil::motion::ClockPolicy> clockPolicy;
  /** THE PROTOCOL'S ENDPOINT, as `--inspect` or `--inspect=PORT` asked
   *  for it: the port to hold, 0 for any free one. Unstated, the window
   *  mounts it on a free port anyway and every other run mounts none. */
  std::optional<uint16_t> inspectPort;
};

/** Reads the whole command line. Nothing when a flag was not understood
 *  or its value could not be read, with what was wrong on stderr; a run
 *  that gets nothing back exits without opening anything. */
std::optional<Arguments> parseArguments(int argc, char* argv[]);
