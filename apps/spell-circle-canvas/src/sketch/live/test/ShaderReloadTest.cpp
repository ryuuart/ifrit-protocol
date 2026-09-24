/** @file
 * A shader beside a guest sketch, as the live host reads it: drawn with,
 * edited and drawn with anew, and an edit that does not compile shown as
 * a failed build is while the sketch runs on with the last program —
 * whether the sketch was compiled in or adopted from a build, and beside
 * a build that failed.
 */

#include <gtest/gtest.h>
#include <include/core/SkBitmap.h>
#include <sigilcompose/core/Core.h>
#include <sigilmaterial/skia/Paint.h>
#include <sigilsketch/canvas/Sketch.h>
#include <sigilsketch/core/Registry.h>
#include <sigilsketch/live/Host.h>

#include <chrono>
#include <filesystem>
#include <fstream>
#include <string>
#include <string_view>
#include <thread>

#include "Fixture.h"
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
        sigil::material::skia::Paint::sksl(
            ctx.assets.shader(ctx.local("fill.sksl")))));
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

TEST(SketchShaderReload, AnEditedShaderIsDrawnAndABrokenOneShownAsABuildIs) {
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

  // A program that does not compile is said where a failed build is, and
  // the sketch keeps drawing with the one that last did.
  rewrite("half4 main(float2 xy) { return notDeclaredAnywhere; }\n", 4);
  pollAfterAWhile(host);
  ASSERT_TRUE(host.live());
  EXPECT_EQ(host.state(), sketch::Host::State::Failed);
  EXPECT_NE(host.errorLog().find("fill.sksl"), std::string::npos)
      << host.errorLog();
  EXPECT_EQ(middleOf(host), SK_ColorGREEN);

  rewrite(solid("0.0, 0.0, 1.0"), 6);
  pollAfterAWhile(host);
  EXPECT_TRUE(host.errorLog().empty()) << host.errorLog();
  EXPECT_EQ(host.state(), sketch::Host::State::Live);
  EXPECT_EQ(middleOf(host), SK_ColorBLUE);
}

/** THE NAME THE ASKING SKETCH READS, which a case changes between two
 *  opens as an author corrects a sketch. */
std::string askedFor = "fil.sksl";

struct AskingByName {
  void setup(sketch::SketchContext& ctx) {
    ctx.canvas(40, 30);
    (void)ctx.assets.shader(ctx.local(askedFor));
  }
};

sketch::Kind askingByNameKind() { return sketch::kindOf<AskingByName>(); }

const sketch::Entry kAskingByName{"asking_by_name", "asking_by_name", "Test",
                                  "", &askingByNameKind};

TEST(SketchShaderReload, ANameTheSketchNoLongerAsksForLeavesTheLog) {
  // A misspelt file corrected in the sketch: once the sketch asks for the
  // right one only, the host is live again although the wrong name was
  // asked for once and is still not there.
  sigil::test::ScratchDir sketches("sigil_sketch_shader_renamed");
  sketches.write("asking_by_name/fill.sksl", solid("1.0, 0.0, 0.0"));
  sketches.write("asking_by_name/asking_by_name.cpp", "// never built\n");

  sketch::Host::Options options;
  options.sketchPath = sketches.path / "asking_by_name" / "asking_by_name.cpp";
  options.assetsDirectory = std::filesystem::temp_directory_path();
  options.sketchesDirectory = sketches.path;
  options.flagsFile = std::filesystem::temp_directory_path() / "no_such.rsp";
  options.compiledIn = &kAskingByName;
  options.clock = sigil::motion::ClockPolicy::Advance;
  askedFor = "fil.sksl";
  sketch::Host host(std::move(options), sketch::test::fonts());
  ASSERT_TRUE(host.live());
  EXPECT_EQ(host.state(), sketch::Host::State::Failed);
  EXPECT_NE(host.errorLog().find("fil.sksl"), std::string::npos)
      << host.errorLog();

  askedFor = "fill.sksl";
  ASSERT_TRUE(host.restartSession());
  EXPECT_TRUE(host.errorLog().empty()) << host.errorLog();
  EXPECT_EQ(host.state(), sketch::Host::State::Live);
}

#ifdef SIGIL_SKETCH_SHADER_GUEST

/** A COMPILER THAT BUILDS NOTHING: an object is an empty file and the
 *  library is the guest image built beside this binary, copied to where
 *  the link names it. While a file named `refuse` stands beside the
 *  script it fails instead, saying so, as a build that did not compile. */
std::string guestCompiler(const std::filesystem::path& script) {
  std::ofstream(script) << "if [ -e \"$(dirname \"$0\")/refuse\" ]; then\n"
                           "  echo 'the stub compiler refused this build'\n"
                           "  exit 1\n"
                           "fi\n"
                           "prev=\n"
                           "for arg in \"$@\"; do\n"
                           "  if [ \"$prev\" = \"-o\" ]; then\n"
                           "    case \"$arg\" in\n"
                           "      *.dylib) cp '" SIGIL_SKETCH_SHADER_GUEST
                           "' \"$arg\" ;;\n"
                           "      *) : > \"$arg\" ;;\n"
                           "    esac\n"
                           "  fi\n"
                           "  prev=$arg\n"
                           "done\n"
                           "exit 0\n";
  return "/bin/sh " + script.string();
}

/** One build of @p host run to its adoption, in a bounded count of turns
 *  so a build that never finishes fails the case instead of hanging it. */
[[nodiscard]] bool buildOnce(sketch::Host& host) {
  host.poll();
  for (int turn = 0; turn < 20000; ++turn) {
    if (!host.compiling()) return true;
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
    host.poll();
  }
  return false;
}

TEST(SketchShaderReload, AnAdoptedBuildShowsWhatItsShadersFoundWrong) {
  sigil::test::ScratchDir sketches("sigil_sketch_shader_adopted");
  const std::filesystem::path shader =
      sketches.path / "shader_asking_guest" / "fill.sksl";
  const std::filesystem::path source =
      sketches.path / "shader_asking_guest" / "shader_asking_guest.cpp";
  const auto stamp = [](const std::filesystem::path& path, int later) {
    std::filesystem::last_write_time(
        path, std::filesystem::file_time_type::clock::now() +
                  std::chrono::seconds(later));
  };
  sketches.write("shader_asking_guest/fill.sksl",
                 "half4 main(float2 xy) { return notDeclaredAnywhere; }\n");
  sketches.write("shader_asking_guest/shader_asking_guest.cpp",
                 "// adopted from the guest image, never compiled\n");

  sketch::Host::Options options;
  options.sketchPath = source;
  options.assetsDirectory = std::filesystem::temp_directory_path();
  options.sketchesDirectory = sketches.path;
  options.flagsFile = std::filesystem::temp_directory_path() / "no_such.rsp";
  options.compiler = guestCompiler(sketches.path / "compiler.sh");
  options.compiledIn = nullptr;
  options.clock = sigil::motion::ClockPolicy::Advance;
  sketch::Host host(std::move(options), sketch::test::fonts());

  // A shader already broken when the build is adopted is said at once.
  ASSERT_TRUE(buildOnce(host)) << "the build never finished";
  ASSERT_TRUE(host.live()) << host.errorLog();
  EXPECT_EQ(host.state(), sketch::Host::State::Failed);
  EXPECT_NE(host.errorLog().find("fill.sksl"), std::string::npos)
      << host.errorLog();

  // …and is said again by the next build, although the shader is broken
  // the same way it was.
  stamp(source, 2);
  ASSERT_TRUE(buildOnce(host)) << "the rebuild never finished";
  EXPECT_EQ(host.generation(), 2);
  EXPECT_EQ(host.state(), sketch::Host::State::Failed);
  EXPECT_NE(host.errorLog().find("fill.sksl"), std::string::npos)
      << host.errorLog();

  // A build that fails says so; the shader mended under it takes back its
  // own words and leaves the build's.
  sketches.write("refuse", "");
  stamp(source, 4);
  ASSERT_TRUE(buildOnce(host)) << "the refused build never finished";
  EXPECT_NE(host.errorLog().find("refused"), std::string::npos)
      << host.errorLog();
  sketches.write("shader_asking_guest/fill.sksl", solid("0.0, 1.0, 0.0"));
  stamp(shader, 6);
  pollAfterAWhile(host);
  EXPECT_NE(host.errorLog().find("refused"), std::string::npos)
      << host.errorLog();
  EXPECT_EQ(host.errorLog().find("fill.sksl"), std::string::npos)
      << host.errorLog();

  std::filesystem::remove(sketches.path / "refuse");
  stamp(source, 8);
  ASSERT_TRUE(buildOnce(host)) << "the mended build never finished";
  EXPECT_TRUE(host.errorLog().empty()) << host.errorLog();
  EXPECT_EQ(host.state(), sketch::Host::State::Live);
}

#endif

}  // namespace
