#pragma once

/** @file
 * What the live host's cases share: the options a host is opened on, over
 * the square this binary carries, and a body whose setup can be counted and
 * refused.
 */

#include <sigilcompose/core/Factories.h>
#include <sigilsketch/core/Registry.h>
#include <sigilsketch/live/Host.h>
#include <sigilsketch/scene2d/Scene2D.h>

#include <filesystem>
#include <stdexcept>

#include "Fixture.h"

namespace sigil::sketch::test {

/** A body whose setup count makes a runtime-session restart observable. */
struct Restarted {
  static inline int setups = 0;
  static inline bool failSetup = false;

  void setup(Scene2DContext& ctx) {
    ++setups;
    if (failSetup) throw std::runtime_error("setup rejected the candidate");
    ctx.canvas(120, 90);
    ctx.composer.render(sigil::compose::box().width(20).height(20));
  }
};

inline Kind restartedKind() { return kindOf<Restarted>(); }
inline const Entry kRestarted{"restarted", "restarted", "Test", "",
                              &restartedKind};

inline Host::Options options(const std::filesystem::path& path) {
  Host::Options opts;
  opts.sketchPath = path;
  opts.assetsDirectory = std::filesystem::temp_directory_path();
  opts.flagsFile = std::filesystem::temp_directory_path() / "no_such.rsp";
  opts.compiledIn = &kSquare;
  return opts;
}

}  // namespace sigil::sketch::test
