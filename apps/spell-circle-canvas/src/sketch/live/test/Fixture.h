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

/** One green square, and nothing else: the body under every host here.
 *  It declares a moment so a host can be asked what the running sketch
 *  wants a still taken at. */
struct Square : Sketch {
  void setup(SketchContext& ctx) override {
    ctx.canvas(120, 90);
    ctx.background({0, 0, 0, 1});
    ctx.captureAt(kMoment);
    ctx.composer.render(compose::box().width(40).height(40).fill(
        compose::Fill::color({0, 1, 0, 1})));
  }

  static constexpr double kMoment = 2.75;
};

inline Kind squareKind() { return kindOf<Square>(); }

/** The registry entry a host is handed to open the square. */
inline const Entry kSquare{"square", "square", "Test", "", &squareKind};

/** A SECOND BODY, told apart from the square by its canvas alone: two
 *  hosts in one process each open one of these, so a case can ask
 *  whether a host still answers with the sketch IT opened after another
 *  host beside it has built. */
struct Wide : Sketch {
  void setup(SketchContext& ctx) override {
    ctx.canvas(200, 100);
    ctx.background({0, 0, 0, 1});
    ctx.composer.render(compose::box().width(60).height(20).fill(
        compose::Fill::color({1, 0, 0, 1})));
  }
};

inline Kind wideKind() { return kindOf<Wide>(); }

inline const Entry kWide{"wide", "wide", "Test", "", &wideKind};

/** A FILE FOR A HOST TO WATCH, in a directory of its own.
 *
 *  The host watches the headers standing beside a sketch as well as the
 *  sketch, so a file dropped straight into the system temp directory
 *  would be watched alongside whatever else happens to be there.
 *
 *  `inADirectoryOfItsOwn` stands the entry in a directory named for it,
 *  which is the OTHER form a sketch takes: there the sources beside it
 *  are units of it rather than other sketches, and that difference is
 *  the whole of what separates the two shapes.
 *
 *  Nothing here is ever compiled: what these tests ask about is a sketch
 *  the binary already carries, which is what makes selecting one
 *  instant. */
struct Watched {
  explicit Watched(std::string_view label, bool inADirectoryOfItsOwn = false)
      : dir(label) {
    path = dir.path / "sketch.cpp";
    if (inADirectoryOfItsOwn) {
      std::filesystem::create_directories(dir.path / "rain");
      path = dir.path / "rain" / "rain.cpp";
    }
    std::ofstream(path) << "// watched, never built\n";
  }

  sigil::test::ScratchDir dir;
  std::filesystem::path path;
};

}  // namespace sigil::sketch::test
