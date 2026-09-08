#pragma once

/** @file
 * The command line as one value: every flag Sketchbook takes, read once
 * into what the lane behind it is asked through.
 */

#include <sigilsketch/plate/Compare.h>
#include <sigilsketch/plate/Story.h>
#include <sigilsketch/plate/Sweep.h>
#include <sigilsketch/plate/Thumbnails.h>

#include <chrono>
#include <filesystem>
#include <optional>
#include <string>

#include "FrameLane.h"
#include "WindowBench.h"

/** WHAT THIS RUN WAS ASKED FOR. One flag names the lane — a comparison,
 *  a listing, the browser's rows, a sweep, a montage, the warm command,
 *  a still, a measurement — and the rest narrow it: which sketches, on
 *  which tier, where the pictures land. */
struct Arguments {
  std::filesystem::path sketchFile;
  std::filesystem::path assetsOverride;
  std::string selected, kind, shotPath;
  sigil::sketch::CompareOptions compareOptions;
  sigil::sketch::SweepOptions sweepOptions;
  sigil::sketch::StoryOptions storyOptions;
  CaptureOptions capture;
  WindowBench windowBench;
  bool headless = false, list = false, catalog = false, gpu = false;
  bool noGpu = false;
  bool warmThumbnails = false;
  bool thumbnailHeavy = false;
  std::chrono::milliseconds thumbnailBudget = sigil::sketch::kThumbnailBudget;
  std::string thumbnailDir;
  std::optional<bool> deterministic;
};

/** Reads the whole command line. Nothing when a flag was not understood
 *  or its value could not be read, with what was wrong on stderr; a run
 *  that gets nothing back exits without opening anything. */
std::optional<Arguments> parseArguments(int argc, char* argv[]);
