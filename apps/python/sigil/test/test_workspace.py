"""Direct native file and folder opening without a Python launcher command."""

import json
import os
import struct
import subprocess
import tempfile
import unittest
import venv
from pathlib import Path


@unittest.skipUnless(
    os.environ.get("SIGIL_TEST_SKETCHBOOK"), "native Sketchbook not supplied"
)
class WorkspaceOpening(unittest.TestCase):
    def setUp(self):
        temporary = tempfile.TemporaryDirectory(prefix="sigil workspace ")
        self.addCleanup(temporary.cleanup)
        self.root = Path(temporary.name).resolve()
        self.host = Path(os.environ["SIGIL_TEST_SKETCHBOOK"]).resolve(strict=True)
        self.environment = os.environ.copy()
        for key in (
            "PYTHONPATH",
            "PYTHONHOME",
            "PYTHONEXECUTABLE",
            "__PYVENV_LAUNCHER__",
        ):
            self.environment.pop(key, None)
        self.environment["UV_OFFLINE"] = "1"

    def run_host(self, *arguments):
        return subprocess.run(
            [str(self.host), *(str(argument) for argument in arguments)],
            cwd=self.root,
            env=self.environment,
            capture_output=True,
            text=True,
            timeout=60,
            check=False,
        )

    def test_direct_native_open_uses_the_sketch_local_venv(self):
        environment = self.root / ".venv"
        venv.EnvBuilder(with_pip=False, symlinks=True).create(environment)
        python = environment / "bin/python"
        site = subprocess.check_output(
            [
                str(python),
                "-I",
                "-c",
                "import sysconfig; print(sysconfig.get_path('purelib'))",
            ],
            text=True,
        ).strip()
        (Path(site) / "workspace_dependency.py").write_text('COLOR = "#8bd0bd"\n')
        source = self.root / "scene.py"
        source.write_text("""import json
import sys
from pathlib import Path
from workspace_dependency import COLOR
from sigil.sketch import sketch

@sketch(size=(96, 64), background=COLOR, capture_at=0)
class Scene:
    def setup(self, ctx):
        Path(__file__).with_suffix(".json").write_text(json.dumps({"prefix": sys.prefix}))
""")
        # The native picker must not inherit another window's active environment.
        self.environment["VIRTUAL_ENV"] = str(self.root / "wrong environment")
        output = self.root / "frame.png"
        result = self.run_host(source, "--frame", output)
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        self.assertEqual(
            json.loads(source.with_suffix(".json").read_text())["prefix"],
            str(environment),
        )
        self.assertEqual(struct.unpack(">II", output.read_bytes()[16:24]), (96, 64))

    def test_the_host_binds_its_own_module_ahead_of_a_copy_in_the_sketch_local_venv(
        self,
    ):
        # An editable install of the package puts a finder for its own compiled
        # copy of the module ahead of the built-in importer. The host's copy is
        # the one its sketches must build with, whatever the environment holds.
        environment = self.root / ".venv"
        venv.EnvBuilder(with_pip=False, symlinks=True).create(environment)
        python = environment / "bin/python"
        site = Path(
            subprocess.check_output(
                [
                    str(python),
                    "-I",
                    "-c",
                    "import sysconfig; print(sysconfig.get_path('purelib'))",
                ],
                text=True,
            ).strip()
        )
        (site / "decoy_sigil.py").write_text("DECOY = True\n")
        (site / "decoy_finder.py").write_text(
            """import importlib.util
import sys
from pathlib import Path

class Finder:
    def find_spec(self, name, path=None, target=None):
        if name != "_sigil":
            return None
        return importlib.util.spec_from_file_location(
            name, Path(__file__).with_name("decoy_sigil.py")
        )

sys.meta_path.insert(0, Finder())
"""
        )
        (site / "decoy_finder.pth").write_text("import decoy_finder\n")
        source = self.root / "scene.py"
        source.write_text("""import json
import sys
from pathlib import Path
import _sigil
from sigil.sketch import sketch

@sketch(size=(96, 64), background='#8bd0bd', capture_at=0)
class Scene:
    def setup(self, ctx):
        Path(__file__).with_suffix(".json").write_text(json.dumps({
            "prefix": sys.prefix,
            "decoy": hasattr(_sigil, "DECOY"),
            "finders": [type(finder).__name__ for finder in sys.meta_path],
        }))
""")
        output = self.root / "frame.png"
        result = self.run_host(source, "--frame", output)
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        report = json.loads(source.with_suffix(".json").read_text())
        self.assertEqual(report["prefix"], str(environment))
        self.assertIn("Finder", report["finders"], "the decoy finder was installed")
        self.assertFalse(report["decoy"], "the sketch built against the host's module")
        self.assertEqual(struct.unpack(">II", output.read_bytes()[16:24]), (96, 64))

    def test_workspace_catalogue_includes_entries_but_skips_helpers_and_nested_projects(
        self,
    ):
        (self.root / "first.py").write_text(
            "@sketch(size=(32, 32))\nclass First: pass\n"
        )
        nested = self.root / "studies"
        nested.mkdir()
        (nested / "second.cpp").write_text(
            'SIGIL_SKETCH(Second, "Study", "A study");\n'
        )
        (self.root / "helper.py").write_text("COLORS = []\n")
        package = self.root / "src" / "art"
        package.mkdir(parents=True)
        (package / "art.py").write_text("COLORS = []\n")
        (package / "__init__.py").write_text("from .art import COLORS\n")
        separate = self.root / "another project"
        separate.mkdir()
        (separate / "pyproject.toml").write_text(
            '[project]\nname="another"\nversion="0.1"\n'
        )
        (separate / "third.py").write_text("@sketch()\nclass Third: pass\n")
        result = self.run_host("--workspace", self.root, "--catalog")
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        rows = [
            json.loads(line)
            for line in result.stdout.splitlines()
            if line.startswith("{")
        ]
        external = [row for row in rows if row["folder"].startswith("Workspace")]
        self.assertEqual(
            {Path(row["path"]).name for row in external}, {"first.py", "second.cpp"}
        )
        self.assertEqual(len(external), 2)
        self.assertEqual(
            {row["entryPath"] for row in external},
            {"first.py", "studies/second.cpp"},
        )
        self.assertTrue(all(row["external"] for row in external))
        self.assertEqual(rows, external)


if __name__ == "__main__":
    unittest.main()
