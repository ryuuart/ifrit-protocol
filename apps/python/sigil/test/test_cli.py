"""Live launch contracts without starting or replacing a native process."""

import io
import json
import os
import platform
import struct
import subprocess
import sys
import sysconfig
import tempfile
import unittest
from contextlib import redirect_stderr, redirect_stdout
from pathlib import Path
from unittest.mock import patch

from sigil import cli


class OpenCommand(unittest.TestCase):
    def setUp(self):
        temporary = tempfile.TemporaryDirectory(prefix="sigil-cli-")
        self.addCleanup(temporary.cleanup)
        self.root = Path(temporary.name).resolve()
        self.work = self.root / "working" / "nested"
        self.work.mkdir(parents=True)
        self.source = self.root / "project with spaces" / "scene.py"
        self.source.parent.mkdir()
        self.source.write_text('"""A launch fixture."""\n')
        self.host = self.executable("native host/Sketchbook")
        self.info = {
            "implementation": "cpython",
            "version": [sys.version_info.major, sys.version_info.minor],
            "soabi": sysconfig.get_config_var("SOABI"),
            "machine": platform.machine(),
            "pointer_bits": struct.calcsize("P") * 8,
        }
        self.enterContext(patch.dict(os.environ, {"PATH": ""}, clear=True))
        self.enterContext(patch.object(cli.Path, "cwd", return_value=self.work))
        self.enterContext(
            patch.object(cli, "__file__", str(self.root / "package/cli.py"))
        )
        self.enterContext(patch.object(cli.sys, "platform", "linux"))
        self.which = self.enterContext(
            patch.object(cli.shutil, "which", return_value=None)
        )
        self.query = self.enterContext(patch.object(cli.subprocess, "run"))
        self.query.return_value = subprocess.CompletedProcess(
            [], 0, stdout=json.dumps(self.info), stderr=""
        )
        self.execute = self.enterContext(patch.object(cli.os, "execve"))

    def executable(self, name):
        path = self.root / name
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_text("#!/bin/sh\nexit 0\n")
        path.chmod(0o755)
        return path

    def assert_error(self, arguments, message):
        output = io.StringIO()
        with redirect_stderr(output), self.assertRaises(SystemExit) as stopped:
            cli.main(arguments)
        self.assertNotEqual(stopped.exception.code, 0)
        self.assertIn(message, output.getvalue())
        self.assertNotIn("Traceback", output.getvalue())
        self.execute.assert_not_called()

    def launch(self, *arguments):
        self.assertEqual(cli.main(["open", str(self.source), *arguments]), 0)
        self.execute.assert_called_once()
        return self.execute.call_args.args

    def test_current_virtual_environment_is_handed_off_without_resolving_its_link(self):
        base = self.executable("base/bin/python")
        interpreter = self.root / ".venv/bin/python"
        interpreter.parent.mkdir(parents=True)
        interpreter.symlink_to(base)
        os.environ.update(
            {
                "VIRTUAL_ENV": str(interpreter.parent.parent),
                "PYTHONEXECUTABLE": "/another/python",
                "__PYVENV_LAUNCHER__": "/another/launcher",
                "KEEP_SETTING": "preserved",
            }
        )
        with patch.object(cli.sys, "executable", str(interpreter)):
            binary, arguments, environment = self.launch("--sketchbook", str(self.host))
        self.assertEqual(binary, str(self.host))
        self.assertEqual(
            arguments,
            [
                str(self.host),
                str(self.source),
                "--python-executable",
                str(interpreter),
                "--python-abi",
                self.info["soabi"],
            ],
        )
        self.assertNotEqual(arguments[3], str(interpreter.resolve()))
        self.assertEqual(environment["VIRTUAL_ENV"], str(interpreter.parent.parent))
        self.assertEqual(environment["KEEP_SETTING"], "preserved")
        self.assertNotIn("PYTHONEXECUTABLE", environment)
        self.assertNotIn("__PYVENV_LAUNCHER__", environment)
        self.assertIn("PYTHONEXECUTABLE", os.environ)
        query_arguments, query_options = self.query.call_args
        self.assertEqual(query_arguments, ([str(self.host), "--python-info"],))
        self.assertEqual(query_options["env"], environment)
        self.assertTrue(query_options["check"])
        self.assertTrue(query_options["capture_output"])
        self.assertGreater(query_options["timeout"], 0)

    def test_incompatible_interpreters_never_launch(self):
        incompatible = {
            "implementation": "pypy",
            "version": [sys.version_info.major, sys.version_info.minor + 1],
            "soabi": "cpython-incompatible",
            "machine": "another-architecture",
            "pointer_bits": 32 if self.info["pointer_bits"] == 64 else 64,
        }
        for field, value in incompatible.items():
            with self.subTest(field=field):
                self.query.return_value.stdout = json.dumps(self.info | {field: value})
                self.assert_error(
                    ["open", str(self.source), "--sketchbook", str(self.host)],
                    f"{field}: host",
                )

    def test_native_machine_aliases_are_compatible(self):
        for current, host in (("aarch64", "ARM64"), ("AMD64", "x86_64")):
            with (
                self.subTest(current=current),
                patch.object(cli.platform, "machine", return_value=current),
            ):
                self.query.return_value.stdout = json.dumps(
                    self.info | {"machine": host}
                )
                self.launch("--sketchbook", str(self.host))
                self.execute.reset_mock()

    def test_invalid_native_handshake_never_launches(self):
        replies = [
            "not JSON",
            "[]",
            "{}",
            json.dumps(self.info | {"version": "3.14"}),
            json.dumps(self.info | {"version": [3, 14, 0]}),
            json.dumps(self.info | {"pointer_bits": "64"}),
        ]
        for reply in replies:
            with self.subTest(reply=reply):
                self.query.return_value.stdout = reply
                self.assert_error(
                    ["open", str(self.source), "--sketchbook", str(self.host)],
                    "Sketchbook returned invalid",
                )

    def test_failed_or_timed_out_handshake_is_a_clean_error(self):
        for error, expected in (
            (
                subprocess.CalledProcessError(2, [], stderr="unknown --python-info"),
                "Could not query Sketchbook",
            ),
            (subprocess.TimeoutExpired([], 10), "within 10 seconds"),
        ):
            with self.subTest(error=error):
                self.query.side_effect = error
                self.assert_error(
                    ["open", str(self.source), "--sketchbook", str(self.host)], expected
                )

    def test_missing_source_fails_before_host_discovery(self):
        self.assert_error(
            ["open", str(self.root / "absent.py")], "Python sketch was not found"
        )
        self.which.assert_not_called()
        self.query.assert_not_called()

    def test_missing_host_explains_how_to_choose_one(self):
        self.assert_error(
            ["open", str(self.source)], "--sketchbook /path/to/Sketchbook"
        )
        self.query.assert_not_called()

    def test_explicit_host_precedes_environment_and_path(self):
        os.environ["SIGIL_SKETCHBOOK"] = str(self.executable("environment/Sketchbook"))
        self.which.return_value = str(self.executable("path/Sketchbook"))
        binary, _, _ = self.launch("--sketchbook", str(self.host))
        self.assertEqual(binary, str(self.host))
        self.which.assert_not_called()

    def test_invalid_explicit_host_does_not_fall_back(self):
        os.environ["SIGIL_SKETCHBOOK"] = str(self.host)
        self.assert_error(
            ["open", str(self.source), "--sketchbook", str(self.root / "missing")],
            "--sketchbook is not an executable file",
        )
        self.query.assert_not_called()

    def test_environment_host_precedes_path(self):
        os.environ["SIGIL_SKETCHBOOK"] = str(self.host)
        self.which.return_value = str(self.executable("path/Sketchbook"))
        binary, _, _ = self.launch()
        self.assertEqual(binary, str(self.host))
        self.which.assert_not_called()

    def test_path_host_precedes_a_local_build(self):
        self.which.return_value = str(self.host)
        self.executable("working/build/bin/Release/Sketchbook")
        binary, _, _ = self.launch()
        self.assertEqual(binary, str(self.host))
        self.which.assert_called_once_with("Sketchbook")

    def test_local_build_lookup_checks_working_source_and_package_ancestors(self):
        names = (
            "working/build/bin/Release/Sketchbook.app/Contents/MacOS/Sketchbook",
            "project with spaces/build/bin/Release/Sketchbook",
            "package/build/bin/Release/Sketchbook",
        )
        hosts = [self.executable(name) for name in names]
        for host in hosts:
            with self.subTest(host=host):
                binary, _, _ = self.launch()
                self.assertEqual(binary, str(host))
                self.execute.reset_mock()
                host.unlink()

    def test_source_package_finds_the_sibling_native_build(self):
        checkout = self.root / "checkout"
        package = checkout / "apps/python/sigil/sigil/cli.py"
        native = self.executable(
            "checkout/apps/spell-circle-canvas/build/bin/Release/"
            "Sketchbook.app/Contents/MacOS/Sketchbook"
        )
        with patch.object(cli, "__file__", str(package)):
            binary, _, _ = self.launch()
        self.assertEqual(binary, str(native))

    def test_source_checkout_host_keeps_existing_discovery_precedence(self):
        package = self.root / "checkout/apps/python/sigil/sigil/cli.py"
        self.executable(
            "checkout/apps/spell-circle-canvas/build/bin/Release/Sketchbook"
        )
        local = self.executable("working/build/bin/Release/Sketchbook")
        with patch.object(cli, "__file__", str(package)):
            for explicit, configured, path, expected in (
                (self.host, self.host, self.host, self.host),
                (None, self.host, self.host, self.host),
                (None, None, self.host, self.host),
                (None, None, None, local),
            ):
                with self.subTest(explicit=explicit, configured=configured, path=path):
                    if configured:
                        os.environ["SIGIL_SKETCHBOOK"] = str(configured)
                    else:
                        os.environ.pop("SIGIL_SKETCHBOOK", None)
                    self.which.return_value = str(path) if path else None
                    arguments = ["--sketchbook", str(explicit)] if explicit else []
                    binary, _, _ = self.launch(*arguments)
                    self.assertEqual(binary, str(expected))
                    self.execute.reset_mock()

    def test_forwarded_native_flags_keep_the_current_environment_arguments(self):
        forwarded = ["--frame", "preview with spaces.png", "--at", "2.5"]
        _, arguments, _ = self.launch("--sketchbook", str(self.host), "--", *forwarded)
        self.assertEqual(arguments[-len(forwarded) :], forwarded)
        self.assertEqual(arguments.count("--python-executable"), 1)
        self.assertEqual(arguments.count("--python-abi"), 1)

    def test_forwarding_cannot_override_the_python_handoff(self):
        for argument in (
            "--python-executable",
            "--python-abi=another-abi",
            "--python-info",
        ):
            with self.subTest(argument=argument):
                self.assert_error(
                    [
                        "open",
                        str(self.source),
                        "--sketchbook",
                        str(self.host),
                        "--",
                        argument,
                    ],
                    "cannot be forwarded",
                )
        self.query.assert_not_called()

    def test_publication_is_a_first_class_live_option(self):
        for name in ("Live Study", "scene.py", "--named", None):
            with self.subTest(name=name):
                flag = f"--publish={name}" if name is not None else "--publish"
                _, arguments, _ = self.launch("--sketchbook", str(self.host), flag)
                self.assertEqual(arguments[-1], flag)
                self.assertEqual(arguments[1], str(self.source))
                self.assertEqual(arguments.count("--python-executable"), 1)
                self.execute.reset_mock()

    def test_empty_and_conflicting_publication_names_are_rejected(self):
        for arguments, message in (
            (["--publish="], "publication name must not be empty"),
            (["--publish", " "], "publication name must not be empty"),
            (["--publish", "live", "--", "--publish", "other"], "only once"),
        ):
            with self.subTest(arguments=arguments):
                self.assert_error(["open", str(self.source), *arguments], message)
        self.query.assert_not_called()


class RenderCommand(unittest.TestCase):
    """A render reads what the sketch declares before it starts a host."""

    def setUp(self):
        temporary = tempfile.TemporaryDirectory(prefix="sigil-render-")
        self.addCleanup(temporary.cleanup)
        self.root = Path(temporary.name).resolve()

    def write(self, *modules):
        """A study that declares `modules` and imports them the way a real
        one does, so a render that opens a host raises from the study's own
        import line."""
        source = self.root / "scene.py"
        declaration = "".join(f'"{module}",' for module in modules)
        imports = "".join(f"import {module}\n" for module in modules)
        source.write_text(
            '"""A study that needs a module."""\n'
            f"REQUIRES = ({declaration})\n"
            f"{imports}"
            "from sigil.compose import box\n"
            "from sigil.sketch import sketch\n"
            "\n\n"
            "@sketch(size=(20, 20), capture_at=0)\n"
            "class Study:\n"
            "    def setup(self, ctx):\n"
            '        ctx.render(box().width(20).height(20).fill("#000000"))\n'
        )
        return source

    def test_a_declared_module_that_is_absent_is_named_and_fails(self):
        source = self.write("_sigil_absent_module_for_render")
        output = self.root / "frame.png"
        stream = io.StringIO()
        with redirect_stderr(stream), self.assertRaises(SystemExit) as stopped:
            cli.main(["render", str(source), "-o", str(output)])
        self.assertNotEqual(stopped.exception.code, 0)
        self.assertIn("_sigil_absent_module_for_render", stream.getvalue())
        # The declaration is read before a host opens, so the study's own
        # import line never runs: the report is the command's, and none of
        # it comes from the failed import or the frames under it.
        self.assertNotIn("ModuleNotFoundError", stream.getvalue())
        self.assertNotIn(str(source), stream.getvalue())
        self.assertFalse(output.exists())

    def test_a_declared_module_this_interpreter_has_still_renders(self):
        source = self.write("math")
        output = self.root / "frame.png"
        with redirect_stdout(io.StringIO()):
            self.assertEqual(cli.main(["render", str(source), "-o", str(output)]), 0)
        self.assertTrue(output.is_file())


if __name__ == "__main__":
    unittest.main()
