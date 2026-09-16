"""Project selection and environment preparation without invoking package installers."""

import io
import json
import os
import subprocess
import sys
import tempfile
import unittest
from contextlib import redirect_stderr, redirect_stdout
from pathlib import Path
from unittest.mock import patch

from sigil import environment


class ProjectEnvironment(unittest.TestCase):
    def setUp(self):
        temporary = tempfile.TemporaryDirectory(prefix="sigil project environment ")
        self.addCleanup(temporary.cleanup)
        self.root = Path(temporary.name).resolve()
        self.project = self.root / "project"
        self.source = self.project / "sketches/scene.py"
        self.source.parent.mkdir(parents=True)
        self.source.write_text("pass\n")
        self.uv = self.executable(self.root / "tools/uv")
        self.pinned = self.executable(self.root / "pinned/bin/python")
        self.info = {
            "implementation": "cpython",
            "version": [3, 14],
            "soabi": "cpython-314-darwin",
            "machine": "arm64",
            "pointer_bits": 64,
        }
        self.options = {
            "version": [3, 14],
            "abi": "cpython-314-darwin",
            "machine": "aarch64",
            "pointer_bits": 64,
            "boundary": self.root,
        }
        self.enterContext(patch.dict(os.environ, {"PATH": "/test/tools"}, clear=True))
        self.which = self.enterContext(
            patch.object(environment.shutil, "which", return_value=str(self.uv))
        )
        self.run = self.enterContext(
            patch.object(environment.subprocess, "run", side_effect=self.process)
        )

    def executable(self, path):
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_text("#!/bin/sh\nexit 0\n")
        path.chmod(0o755)
        return path

    def project_file(self, root=None, content=None):
        root = self.project if root is None else root
        path = root / "pyproject.toml"
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_text(content or '[project]\nname = "drawing"\nversion = "0.1.0"\n')
        return path

    def venv(self, root=None):
        root = self.project if root is None else root
        executable = self.executable(root / ".venv/bin/python")
        (root / ".venv/pyvenv.cfg").write_text("home = /base/python\n")
        return executable

    def process(self, arguments, **options):
        if arguments[1] == "sync":
            self.venv(Path(arguments[arguments.index("--project") + 1]))
            output = "prepared dependencies"
        elif arguments[1:3] == ["python", "find"]:
            output = str(self.pinned)
        else:
            output = json.dumps(self.info)
        return subprocess.CompletedProcess(arguments, 0, stdout=output, stderr="")

    def resolve(self, source=None, **options):
        return environment.resolve(
            self.source if source is None else source, **(self.options | options)
        )

    def syncs(self):
        return [call for call in self.run.call_args_list if call.args[0][1] == "sync"]

    def test_source_without_project_uses_the_host_environment(self):
        self.assertEqual(self.resolve(), {"executable": "", "abi": ""})
        self.which.assert_not_called()
        self.run.assert_not_called()

    def test_tool_only_pyproject_is_ignored_while_walking_up(self):
        self.project_file(self.source.parent, "[tool.ruff]\nline-length = 88\n")
        self.project_file()
        result = self.resolve()
        self.assertEqual(result["executable"], str(self.project / ".venv/bin/python"))
        self.assertEqual(self.syncs()[0].kwargs["cwd"], self.project)

    def test_nearest_python_project_wins_and_folder_inputs_work(self):
        self.project_file()
        self.project_file(self.source.parent)
        result = self.resolve(self.source.parent)
        self.assertEqual(
            result["executable"], str(self.source.parent / ".venv/bin/python")
        )
        self.assertEqual(self.syncs()[0].kwargs["cwd"], self.source.parent)

    def test_uv_configuration_alone_defines_a_project(self):
        self.project_file(content="[tool.uv]\npackage = false\n")
        self.assertEqual(self.resolve()["abi"], self.options["abi"])
        self.assertEqual(len(self.syncs()), 1)

    def test_boundary_prevents_discovering_the_application_project(self):
        self.project_file(self.root)
        self.assertEqual(
            self.resolve(boundary=self.project), {"executable": "", "abi": ""}
        )
        self.run.assert_not_called()

    def test_boundary_includes_its_own_project(self):
        self.project_file()
        self.assertTrue(self.resolve(boundary=self.project)["executable"])

    def test_source_outside_explicit_boundary_is_rejected(self):
        boundary = self.root / "other"
        boundary.mkdir()
        with self.assertRaisesRegex(ValueError, "outside the project search boundary"):
            self.resolve(boundary=boundary)
        self.run.assert_not_called()

    def test_existing_plain_venv_needs_no_uv_and_keeps_its_symlink(self):
        interpreter = self.venv()
        interpreter.unlink()
        interpreter.symlink_to(self.pinned)
        result = self.resolve()
        self.assertEqual(
            result, {"executable": str(interpreter), "abi": self.options["abi"]}
        )
        self.assertNotEqual(result["executable"], str(interpreter.resolve()))
        self.which.assert_not_called()
        self.assertEqual(self.run.call_count, 1)
        self.assertIn("-I", self.run.call_args.args[0])

    def test_incompatible_existing_venv_fails_before_uv_can_change_it(self):
        self.project_file()
        interpreter = self.venv()
        original = interpreter.read_bytes()
        self.info["version"] = [3, 13]
        with self.assertRaisesRegex(ValueError, "incompatible with Sketchbook"):
            self.resolve()
        self.which.assert_not_called()
        self.assertEqual(len(self.syncs()), 0)
        self.assertEqual(interpreter.read_bytes(), original)

    def test_existing_project_is_checked_before_sync_and_not_retargeted(self):
        self.project_file()
        interpreter = self.venv()
        self.resolve()
        self.assertEqual(self.run.call_args_list[0].args[0][0], str(interpreter))
        self.assertEqual(self.run.call_args_list[1].args[0][1], "sync")
        self.assertNotIn("--python", self.syncs()[0].args[0])

    def test_fresh_project_sync_preserves_extras_and_skips_the_native_package(self):
        project = self.project_file(
            content='[project]\nname="drawing"\nversion="0.1.0"\nrequires-python=">=3.12"\n'
        )
        original = project.read_bytes()
        self.resolve()
        arguments = self.syncs()[0].args[0]
        self.assertIn("--inexact", arguments)
        self.assertIn("--no-install-project", arguments)
        self.assertEqual(
            arguments[arguments.index("--no-install-package") + 1], "sigil-sketch"
        )
        self.assertEqual(arguments[arguments.index("--python") + 1], "3.14")
        self.assertEqual(project.read_bytes(), original)

    def test_compatible_pin_is_respected_and_checked_before_sync(self):
        self.project_file()
        (self.project / ".python-version").write_text("3.14.3\n")
        self.resolve()
        calls = self.run.call_args_list
        self.assertEqual(calls[0].args[0][1:3], ["python", "find"])
        self.assertEqual(calls[1].args[0][0], str(self.pinned))
        self.assertEqual(calls[2].args[0][1], "sync")
        self.assertNotIn("--python", self.syncs()[0].args[0])

    def test_wrong_version_pin_does_not_replace_a_compatible_environment(self):
        self.project_file()
        self.venv()
        (self.project / ".python-version").write_text("3.13\n")
        with self.assertRaisesRegex(ValueError, "requests Python 3.13"):
            self.resolve()
        self.assertEqual(self.run.call_count, 1)
        self.which.assert_not_called()
        self.assertEqual(len(self.syncs()), 0)

    def test_complex_pin_with_incompatible_abi_is_rejected_before_sync(self):
        self.project_file()
        (self.project / ".python-version").write_text("3.14+freethreaded\n")
        self.info["soabi"] = "cpython-314t-darwin"
        with self.assertRaisesRegex(ValueError, "soabi: project"):
            self.resolve()
        self.assertEqual(len(self.syncs()), 0)

    def test_sync_failure_keeps_the_package_manager_diagnostic(self):
        self.project_file()
        self.run.side_effect = subprocess.CalledProcessError(
            1, [], stderr="No solution found: package requires Python >=3.15"
        )
        with self.assertRaisesRegex(RuntimeError, "No solution found"):
            self.resolve()

    def test_package_manager_timeout_is_bounded(self):
        self.project_file()
        self.run.side_effect = subprocess.TimeoutExpired([], 120)
        with self.assertRaisesRegex(RuntimeError, "timed out after 120 seconds"):
            self.resolve()
        self.assertEqual(self.run.call_args.kwargs["timeout"], 120)

    def test_missing_uv_does_not_trigger_an_install(self):
        self.project_file()
        self.which.return_value = None
        with (
            patch.object(environment.os, "access", return_value=False),
            self.assertRaisesRegex(FileNotFoundError, "needs uv"),
        ):
            self.resolve()
        self.run.assert_not_called()

    def test_standard_user_uv_location_is_found_without_path(self):
        self.project_file()
        self.which.return_value = None
        fallback = self.executable(self.root / "home/.local/bin/uv")
        with patch.object(environment.Path, "home", return_value=self.root / "home"):
            self.resolve()
        self.assertEqual(self.syncs()[0].args[0][0], str(fallback))

    def test_subprocesses_ignore_another_windows_environment_but_keep_index_config(
        self,
    ):
        self.project_file()
        rejected = (
            "VIRTUAL_ENV",
            "PYTHONHOME",
            "PYTHONPATH",
            "PYTHONEXECUTABLE",
            "__PYVENV_LAUNCHER__",
            "UV_PROJECT",
            "UV_PROJECT_ENVIRONMENT",
            "UV_PYTHON",
            "UV_WORKING_DIR",
        )
        os.environ.update(dict.fromkeys(rejected, "/another/window"))
        os.environ["UV_CACHE_DIR"] = "/a/cache"
        os.environ["UV_INDEX_URL"] = "https://packages.example.test/simple"
        self.resolve()
        for call in self.run.call_args_list:
            variables = call.kwargs["env"]
            self.assertTrue(set(rejected).isdisjoint(variables))
            self.assertEqual(variables["UV_CACHE_DIR"], "/a/cache")
            self.assertEqual(
                variables["UV_INDEX_URL"], "https://packages.example.test/simple"
            )
            self.assertEqual(call.kwargs["cwd"], self.project)
        self.assertIn("VIRTUAL_ENV", os.environ)

    def test_malformed_project_configuration_is_reported(self):
        self.project_file(content="[project\n")
        with self.assertRaisesRegex(ValueError, "Invalid project configuration"):
            self.resolve()
        self.run.assert_not_called()

    def test_malformed_interpreter_information_is_not_accepted(self):
        self.venv()
        for response in ("not json", "[]", "{}", '{"version":"3.14"}'):
            with self.subTest(response=response):
                self.run.side_effect = None
                self.run.return_value = subprocess.CompletedProcess(
                    [], 0, stdout=response
                )
                with self.assertRaisesRegex(ValueError, "invalid compatibility"):
                    self.resolve()

    def test_cli_stdout_is_only_the_handoff_json(self):
        self.project_file()
        output = io.StringIO()
        with redirect_stdout(output):
            result = environment.main(
                [
                    str(self.source),
                    "--version",
                    "3.14",
                    "--abi",
                    self.options["abi"],
                    "--machine",
                    "arm64",
                    "--pointer-bits",
                    "64",
                    "--boundary",
                    str(self.root),
                ]
            )
        self.assertEqual(result, 0)
        self.assertEqual(json.loads(output.getvalue())["abi"], self.options["abi"])

    def test_cli_errors_use_stderr_and_nonzero_status(self):
        output, errors = io.StringIO(), io.StringIO()
        with (
            redirect_stdout(output),
            redirect_stderr(errors),
            self.assertRaises(SystemExit) as stopped,
        ):
            environment.main(
                [
                    str(self.root / "missing.py"),
                    "--version",
                    "3.14",
                    "--abi",
                    self.options["abi"],
                    "--machine",
                    "arm64",
                    "--pointer-bits",
                    "64",
                ]
            )
        self.assertNotEqual(stopped.exception.code, 0)
        self.assertEqual(output.getvalue(), "")
        self.assertIn("sigil environment:", errors.getvalue())


class StandaloneEnvironment(unittest.TestCase):
    def test_isolated_script_needs_no_package_or_sketch_import(self):
        with tempfile.TemporaryDirectory() as folder:
            source = Path(folder) / "scene.py"
            source.write_text(
                'raise AssertionError("The resolver must not execute sketches")\n'
            )
            result = subprocess.run(
                [
                    sys.executable,
                    "-I",
                    str(Path(environment.__file__).resolve()),
                    str(source),
                    "--version",
                    "3.14",
                    "--abi",
                    "cpython-314-darwin",
                    "--machine",
                    "arm64",
                    "--pointer-bits",
                    "64",
                    "--boundary",
                    folder,
                ],
                cwd=folder,
                capture_output=True,
                text=True,
                check=True,
                timeout=10,
            )
            self.assertEqual(json.loads(result.stdout), {"executable": "", "abi": ""})
            self.assertEqual(result.stderr, "")


if __name__ == "__main__":
    unittest.main()
