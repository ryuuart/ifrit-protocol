/** @file
 * A shader file beside a sketch, read through the host's hub by
 * Material's shader: drawn with, edited, and drawn with anew once the
 * host's poll sees the edit and the sketch declares itself again.
 */

#include <gtest/gtest.h>
#include <include/core/SkBitmap.h>
#include <sigilcompose/core/Core.h>
#include <sigilmaterial/program/Shader.h>
#include <sigilsketch/canvas/Sketch.h>
#include <sigilsketch/core/Registry.h>
#include <sigilsketch/live/Host.h>

#include <chrono>
#include <filesystem>
#include <fstream>
#include <string>
#include <string_view>
#include <thread>

#include "ScratchDir.h"
#include "support/Fixtures.h"

namespace {

namespace sketch = sigil::sketch;

/** A GUEST THAT PAINTS ITSELF WITH ITS OWN FILE: the whole canvas filled
 *  with the program in `fill.sksl` beside it. */
struct ShaderGuest {
  void setup(sketch::SketchContext& ctx) {
    ctx.canvas(40, 30);
    ctx.background({0, 0, 0, 1});
    ctx.composer.render(sigil::compose::box().width(40).height(30).fill(
        sigil::material::shader(ctx.assets.hub(), ctx.local("fill.sksl"))));
  }
};

sketch::Kind shaderGuestKind() { return sketch::kindOf<ShaderGuest>(); }

const sketch::Entry kShaderGuest{"shader_guest", "shader_guest", "Test", "",
                                 &shaderGuestKind};

std::string solid(std::string_view rgb) {
  return "half4 main(float2 xy) { return half4(" + std::string(rgb) +
         ", 1.0); }\n";
}

/** The colour at the middle of the host's still. */
SkColor middleOf(sketch::Host& host) {
  const SkBitmap still = host.still(1.0f);
  if (still.isNull()) return SK_ColorTRANSPARENT;
  return still.getColor(still.width() / 2, still.height() / 2);
}

/** Frames enough to reach the host's next asset poll, then the poll. */
void pollAfterAWhile(sketch::Host& host) {
  for (int frame = 0; frame < 40; ++frame) ASSERT_TRUE(host.frame(1.0 / 60.0));
  host.poll();
}

TEST(SketchShaderReload, AnEditedShaderIsDrawnAnew) {
  sigil::test::ScratchDir sketches("sigil_sketch_shader_reload");
  const std::string file = "shader_guest/fill.sksl";
  const auto rewrite = [&](std::string_view body, int later) {
    sketches.write(file, body);
    std::filesystem::last_write_time(
        sketches.path / file, std::filesystem::file_time_type::clock::now() +
                                  std::chrono::seconds(later));
  };
  sketches.write(file, solid("1.0, 0.0, 0.0"));
  sketches.write("shader_guest/shader_guest.cpp", "// watched, never built\n");

  sketch::Host::Options options;
  options.sketchPath = sketches.path / "shader_guest" / "shader_guest.cpp";
  options.assetsDirectory = std::filesystem::temp_directory_path();
  options.sketchesDirectory = sketches.path;
  options.flagsFile = std::filesystem::temp_directory_path() / "no_such.rsp";
  options.compiledIn = &kShaderGuest;
  options.clock = sigil::motion::ClockPolicy::Advance;
  sketch::Host host(std::move(options), sketch::test::fonts());
  ASSERT_TRUE(host.live());
  EXPECT_EQ(middleOf(host), SK_ColorRED);
  EXPECT_TRUE(host.errorLog().empty()) << host.errorLog();

  rewrite(solid("0.0, 1.0, 0.0"), 2);
  pollAfterAWhile(host);
  EXPECT_EQ(middleOf(host), SK_ColorGREEN);

  rewrite(solid("0.0, 0.0, 1.0"), 4);
  pollAfterAWhile(host);
  EXPECT_TRUE(host.errorLog().empty()) << host.errorLog();
  EXPECT_EQ(host.state(), sketch::Host::State::Live);
  EXPECT_EQ(middleOf(host), SK_ColorBLUE);
}

}  // namespace
