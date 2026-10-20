/** @file
 * A Python source through the live host's importer: refused with the
 * configuration it lacks, opened as the native kind an importer provides,
 * and reading the assets of its workspace or of the root above it.
 */

#include <gtest/gtest.h>
#include <sigilsketch/live/Host.h>

#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <string>
#include <utility>

#include "HostTestSupport.h"
#include "support/Fixtures.h"

namespace {

using namespace sigil::sketch;
using sigil::sketch::test::fonts;
using sigil::sketch::test::options;
using sigil::sketch::test::Restarted;
using sigil::sketch::test::Watched;

TEST(SketchHost, MissingPythonImporterReportsTheConfigurationWithoutCompiling) {
  const Watched file("sigil_sketch_host_python_disabled");
  auto python = file.path;
  python.replace_extension(".py");
  std::ofstream(python) << "pass\n";
  Host::Options opts = options(python);
  opts.compiledIn = nullptr;
  Host host(std::move(opts), fonts());
  host.poll();
  EXPECT_FALSE(host.compiling());
  EXPECT_FALSE(host.live());
  EXPECT_NE(host.errorLog().find("Host::Options::pythonLoader"),
            std::string::npos);
}

TEST(SketchHost,
     PythonImporterProvidesANativeKindWithoutCompilerOrInterpreter) {
  const Watched file("sigil_sketch_host_python_importer");
  auto python = file.path;
  python.replace_extension(".py");
  std::ofstream(python) << "source handled by importer\n";
  Host::Options opts = options(python);
  opts.compiledIn = nullptr;
  opts.pythonLoader = [](const std::filesystem::path& source) {
    if (source.extension() != ".py") throw std::runtime_error("wrong source");
    return kindOf<Restarted>();
  };
  Restarted::setups = 0;
  Restarted::failSetup = false;
  Host host(std::move(opts), fonts());
  host.poll();
  EXPECT_FALSE(host.compiling());
  EXPECT_TRUE(host.live());
  EXPECT_EQ(Restarted::setups, 1);
  EXPECT_EQ(host.canvasSize(), SkSize::Make(120, 90));
  EXPECT_TRUE(host.errorLog().empty());
}

/** A body that reads one file through `res://` and keeps what it read. */
struct ReadsTheNote {
  static inline std::string read;

  void setup(Scene2DContext& ctx) {
    ctx.canvas(120, 90);
    auto bytes = ctx.assets.hub().read("res://note.txt");
    read = bytes ? std::string(bytes->asText()) : std::string();
    ctx.composer.render(sigil::compose::box().width(20).height(20));
  }
};

TEST(SketchHost, AWorkspaceMountsItsOwnAssetsAtRes) {
  // The layout `sigil.py workspace new <dir>` writes: the entry named for
  // its folder, and `assets/` inside that folder. A shared `assets/` one
  // level up holds a file of the same name, so a read that went there is
  // told apart from the one that should be made.
  const sigil::test::ScratchDir root("sigil_sketch_workspace_assets");
  const auto workspace = root.path / "aurora";
  std::filesystem::create_directories(workspace / "assets");
  std::filesystem::create_directories(root.path / "assets");
  std::ofstream(workspace / "assets" / "note.txt") << "the workspace's own";
  std::ofstream(root.path / "assets" / "note.txt") << "the shared root";
  const auto entry = workspace / "aurora.py";
  std::ofstream(entry) << "source handled by importer\n";

  Host::Options opts;
  opts.sketchPath = entry;
  opts.flagsFile = root.path / "no_such.rsp";
  opts.pythonLoader = [](const std::filesystem::path&) {
    return kindOf<ReadsTheNote>();
  };
  ReadsTheNote::read.clear();
  Host host(std::move(opts), fonts());
  host.poll();
  ASSERT_TRUE(host.live()) << host.errorLog();
  EXPECT_EQ(ReadsTheNote::read, "the workspace's own");
}

TEST(SketchHost, ADirectorySketchWithoutAssetsSharesTheRootAboveIt) {
  const sigil::test::ScratchDir root("sigil_sketch_shared_assets");
  const auto directory = root.path / "aurora";
  std::filesystem::create_directories(directory);
  std::filesystem::create_directories(root.path / "assets");
  std::ofstream(root.path / "assets" / "note.txt") << "the shared root";
  const auto entry = directory / "aurora.py";
  std::ofstream(entry) << "source handled by importer\n";

  Host::Options opts;
  opts.sketchPath = entry;
  opts.flagsFile = root.path / "no_such.rsp";
  opts.pythonLoader = [](const std::filesystem::path&) {
    return kindOf<ReadsTheNote>();
  };
  ReadsTheNote::read.clear();
  Host host(std::move(opts), fonts());
  host.poll();
  ASSERT_TRUE(host.live()) << host.errorLog();
  EXPECT_EQ(ReadsTheNote::read, "the shared root");
}

}  // namespace
