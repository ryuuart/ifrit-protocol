"""Which targets a test filter needs, asked of a tree that is not built.

A tree configured and not built registers a placeholder per test binary
and no case names, so a filter over suite names finds nothing in it. The
fixture here is a small project with two GoogleTest binaries discovered
the way the library tree discovers its own, configured twice: a primary
tree that is built and a lane tree that is only configured.
"""

import shutil
import subprocess
import tempfile
import unittest
from pathlib import Path

from sigil import tree

PROJECT = """\
cmake_minimum_required(VERSION 3.25)
project(fixture CXX)
set(CMAKE_CXX_STANDARD 20)
find_package(GTest CONFIG REQUIRED)
include(GoogleTest)
enable_testing()
foreach(name alpha beta)
  add_executable(fixture_${name}_test ${name}.cpp)
  target_link_libraries(fixture_${name}_test PRIVATE GTest::gtest_main)
  set_target_properties(fixture_${name}_test PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin/$<CONFIG>/tests)
  gtest_discover_tests(fixture_${name}_test DISCOVERY_MODE PRE_TEST)
endforeach()
"""

CASE = """\
#include <gtest/gtest.h>
TEST({suite}, Holds) {{ EXPECT_TRUE(true); }}
"""


def dependencies() -> Path | None:
    """The primary tree's installed packages, where GoogleTest is."""
    installed = tree.build_dir() / "vcpkg_installed"
    for triplet in sorted(installed.glob("*")):
        if (triplet / "share" / "gtest").is_dir():
            return triplet
    return None


@unittest.skipUnless(
    shutil.which("cmake") and shutil.which("ctest") and shutil.which("ninja"),
    "needs cmake, ctest and ninja",
)
@unittest.skipUnless(dependencies(), "needs the primary tree's GoogleTest")
class AFilterOverSuiteNamesOnAFreshLaneTree(unittest.TestCase):
    @classmethod
    def setUpClass(cls) -> None:
        cls.work = tempfile.TemporaryDirectory(prefix="sigil-tree-")
        root = Path(cls.work.name)
        source = root / "source"
        source.mkdir()
        (source / "CMakeLists.txt").write_text(PROJECT)
        (source / "alpha.cpp").write_text(CASE.format(suite="Alpha"))
        (source / "beta.cpp").write_text(CASE.format(suite="Beta"))
        cls.primary = root / "build"
        cls.lane = root / "build-asan"
        for directory in (cls.primary, cls.lane):
            subprocess.run(
                [
                    "cmake",
                    "-S",
                    source,
                    "-B",
                    directory,
                    "-G",
                    "Ninja Multi-Config",
                    f"-DCMAKE_PREFIX_PATH={dependencies()}",
                ],
                check=True,
                capture_output=True,
            )
        subprocess.run(
            ["cmake", "--build", cls.primary, "--config", "Release"],
            check=True,
            capture_output=True,
        )

    @classmethod
    def tearDownClass(cls) -> None:
        cls.work.cleanup()

    def test_the_lane_alone_names_no_case(self) -> None:
        self.assertEqual(
            tree.registered_tests(self.lane, "RelWithDebInfo", r"^Alpha\."), []
        )

    def test_the_suites_binary_is_read_from_the_primary_tree(self) -> None:
        self.assertEqual(
            tree.build_targets(
                self.lane, "RelWithDebInfo", r"^Alpha\.", primary=self.primary
            ),
            ["fixture_alpha_test"],
        )
        self.assertEqual(
            tree.build_targets(
                self.lane, "RelWithDebInfo", r"^(Alpha|Beta)\.", primary=self.primary
            ),
            ["fixture_alpha_test", "fixture_beta_test"],
        )

    def test_a_filter_naming_the_binary_still_finds_its_placeholder(self) -> None:
        self.assertEqual(
            tree.build_targets(
                self.lane, "RelWithDebInfo", "fixture_beta_test", primary=self.primary
            ),
            ["fixture_beta_test"],
        )


if __name__ == "__main__":
    unittest.main()
