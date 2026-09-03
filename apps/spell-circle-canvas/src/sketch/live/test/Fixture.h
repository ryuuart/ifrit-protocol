#pragma once

/** @file
 * What both halves of the live host's tests hold: a sketch this binary
 * already carries, and a file on disk for a host to watch it through.
 */

#include <sigilsketch/canvas/Sketch.h>
#include <sigilsketch/core/Registry.h>

#include <filesystem>
#include <fstream>
#include <string_view>

#include "ScratchDir.h"

namespace sigil::sketch::test {

/** One green square, and nothing else: the body under every host here. */
struct Square : Sketch {
  void setup(SketchContext& ctx) override {
    ctx.canvas(120, 90);
    ctx.background({0, 0, 0, 1});
    ctx.composer.render(compose::box().width(40).height(40).fill(
        compose::Fill::color({0, 1, 0, 1})));
  }
};

inline Kind squareKind() { return kindOf<Square>(); }

/** The registry entry a host is handed to open the square. */
inline const Entry kSquare{"square", "square", "Test", "", &squareKind};

/** A FILE FOR A HOST TO WATCH, in a directory of its own.
 *
 *  The host watches the headers standing beside a sketch as well as the
 *  sketch, so a file dropped straight into the system temp directory
 *  would be watched alongside whatever else happens to be there.
 *
 *  Nothing here is ever compiled: what these tests ask about is a sketch
 *  the binary already carries, which is what makes selecting one
 *  instant. */
struct Watched {
  explicit Watched(std::string_view label) : dir(label) {
    path = dir.path / "sketch.cpp";
    std::ofstream(path) << "// watched, never built\n";
  }

  sigil::test::ScratchDir dir;
  std::filesystem::path path;
};

/** THE DIRECTORY FORM of the same: the entry standing in a directory of
 *  its own name, so the sources beside it are units of it rather than
 *  other sketches. */
struct WatchedDirectory {
  explicit WatchedDirectory(std::string_view label) : dir(label) {
    std::filesystem::create_directories(dir.path / "rain");
    path = dir.path / "rain" / "rain.cpp";
    std::ofstream(path) << "// watched, never built\n";
  }

  sigil::test::ScratchDir dir;
  std::filesystem::path path;
};

}  // namespace sigil::sketch::test
