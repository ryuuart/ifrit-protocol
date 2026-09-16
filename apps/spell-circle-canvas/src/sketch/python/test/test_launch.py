"""Native launch integration using a temporary, dependency-free virtual environment."""

import json
import os
import struct
import subprocess
import sys
import tempfile
import textwrap
import unittest
import venv
from pathlib import Path

import sigil


@unittest.skipUnless(
    os.environ.get("SIGIL_TEST_SKETCHBOOK"), "native Sketchbook not supplied"
)
class NativeLaunch(unittest.TestCase):
    def setUp(self):
        temporary = tempfile.TemporaryDirectory(prefix="sigil native launch ")
        self.addCleanup(temporary.cleanup)
        self.root = Path(temporary.name).resolve()
        self.host = Path(os.environ["SIGIL_TEST_SKETCHBOOK"]).resolve(strict=True)
        self.environment = os.environ.copy()
        for name in (
            "PYTHONHOME",
            "PYTHONPATH",
            "PYTHONEXECUTABLE",
            "__PYVENV_LAUNCHER__",
        ):
            self.environment.pop(name, None)

    def run_process(self, arguments, *, environment=None):
        return subprocess.run(
            [str(argument) for argument in arguments],
            cwd=self.root,
            env=self.environment if environment is None else environment,
            capture_output=True,
            text=True,
            timeout=60,
            check=False,
        )

    def test_open_keeps_site_hooks_prefix_and_child_interpreter_in_the_venv(self):
        environment_root = self.root / "project environment"
        venv.EnvBuilder(with_pip=False, symlinks=True).create(environment_root)
        interpreter = environment_root / (
            "Scripts/python.exe" if os.name == "nt" else "bin/python"
        )
        located = self.run_process(
            [
                interpreter,
                "-I",
                "-c",
                "import sysconfig; print(sysconfig.get_path('purelib'))",
            ]
        )
        self.assertEqual(located.returncode, 0, located.stdout + located.stderr)
        site_packages = Path(located.stdout.strip())
        self.assertTrue(site_packages.is_relative_to(environment_root))
        support = self.root / "environment only modules"
        support.mkdir()
        module = support / "_sigil_launch_environment_only.py"
        module.write_text('VALUE = "visible through the virtual environment"\n')
        (site_packages / "sigil_launch_test.pth").write_text(
            str(support) + "\nimport sys; sys.dont_write_bytecode = True; "
            "sys._sigil_launch_pth_loaded = True\n"
        )
        source = self.root / "environment sketch.py"
        source.write_text(
            textwrap.dedent("""
                import json
                import subprocess
                import sys
                from pathlib import Path
                import _sigil_launch_environment_only as dependency
                from sigil.compose import box
                from sigil.sketch import sketch

                CHILD = (
                    "import json, sys; "
                    "import _sigil_launch_environment_only as dependency; "
                    "print(json.dumps({'prefix': sys.prefix, "
                    "'executable': sys.executable, 'value': dependency.VALUE, "
                    "'pth': sys._sigil_launch_pth_loaded}))"
                )

                @sketch(size=(96, 64), background="#142333", capture_at=0)
                class EnvironmentSketch:
                    def setup(self, ctx):
                        child = subprocess.run(
                            [sys.executable, "-I", "-c", CHILD],
                            capture_output=True, text=True, check=True, timeout=20,
                        )
                        evidence = {
                            "prefix": sys.prefix,
                            "executable": sys.executable,
                            "value": dependency.VALUE,
                            "module": dependency.__file__,
                            "pth": sys._sigil_launch_pth_loaded,
                            "isolated": sys.flags.isolated,
                            "ignore_environment": sys.flags.ignore_environment,
                            "child": json.loads(child.stdout),
                        }
                        Path(__file__).with_suffix(".json").write_text(json.dumps(evidence))
                        ctx.render(box(width=96, height=64, fill="#8bd0bd"))
                """)
        )
        output = self.root / "rendered frame.png"
        launch_environment = self.environment | {
            "PYTHONPATH": str(Path(sigil.__file__).resolve().parent.parent),
            "PYTHONDONTWRITEBYTECODE": "1",
        }
        result = self.run_process(
            [
                interpreter,
                "-m",
                "sigil",
                "open",
                source,
                "--sketchbook",
                self.host,
                "--",
                "--frame",
                output,
            ],
            environment=launch_environment,
        )
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        evidence = json.loads(source.with_suffix(".json").read_text())
        self.assertEqual(evidence["prefix"], str(environment_root))
        self.assertEqual(evidence["executable"], str(interpreter))
        self.assertEqual(evidence["value"], "visible through the virtual environment")
        self.assertEqual(Path(evidence["module"]), module)
        self.assertTrue(evidence["pth"])
        self.assertEqual(evidence["isolated"], 1)
        self.assertEqual(evidence["ignore_environment"], 1)
        self.assertEqual(
            evidence["child"],
            {
                "prefix": str(environment_root),
                "executable": str(interpreter),
                "value": "visible through the virtual environment",
                "pth": True,
            },
        )
        image = output.read_bytes()
        self.assertEqual(image[:8], b"\x89PNG\r\n\x1a\n")
        self.assertEqual(struct.unpack(">II", image[16:24]), (96, 64))

    def test_native_abi_mismatch_precedes_loading_the_sketch(self):
        source = self.root / "invalid sketch.py"
        source.write_text("raise RuntimeError('SKETCH_MUST_NOT_LOAD')\n")
        output = self.root / "unwritten.png"
        result = self.run_process(
            [
                self.host,
                source,
                "--python-executable",
                os.path.abspath(sys.executable),
                "--python-abi",
                "incompatible-test-abi",
                "--frame",
                output,
            ]
        )
        self.assertNotEqual(result.returncode, 0)
        self.assertIn("--python-abi mismatch", result.stderr)
        self.assertNotIn("SKETCH_MUST_NOT_LOAD", result.stdout + result.stderr)
        self.assertFalse(output.exists())


if __name__ == "__main__":
    unittest.main()
