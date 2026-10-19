"""The prompt ``sigil repl`` reads, driven against a host answered in this
process: what it sends for each input, and what it prints of the answer."""

import contextlib
import io
import json
import os
import tempfile
import traceback
import unittest
from pathlib import Path

from sigil import cli
from sigil._console import SketchConsole, evaluationMode
from sigil.protocol import Envelopes, Session
from sigil.protocol.session import EvaluationMode


class StubHost(Envelopes):
    """A host whose session runs evaluated text in a namespace of its own,
    as Grimoire's evaluate door does inside a sketch, and writes a still
    under a directory of its own."""

    def __init__(self, stills: Path, *, door: bool = True) -> None:
        super().__init__()
        self.stills = stills
        self.door = door
        self.namespace: dict[str, object] = {}
        self.requests: list[dict[str, object]] = []
        self.watched: Path | None = None
        self.stamp: int | None = None
        self.imports = 0
        self.load()

    def load(self) -> None:
        """Imports the sketch again: a fresh instance of a class whose
        module is named for the import, as the host's loader names it."""
        self.imports += 1
        body = type("Hello", (), {"__module__": f"_sigil_guest_{self.imports}.hello"})
        self.namespace["self"] = body()

    def poll(self) -> None:
        """The host's watch: an edit to the file is imported again."""
        if self.watched is None:
            return
        stamp = self.watched.stat().st_mtime_ns
        if stamp != self.stamp:
            if self.stamp is not None:
                self.load()
            self.stamp = stamp

    def exchange(self, envelope: str) -> str:
        self.poll()
        request = json.loads(envelope)
        self.requests.append(request)
        method = request["method"]
        parameters = request.get("parameters", {})
        if method == "session.evaluate":
            if not self.door:
                return self.refuse(request, "session.evaluate: this host evaluates no text")
            return self.answer(request, self.evaluate(parameters))
        if method == "session.still":
            path = self.stills / "still-0001.png"
            path.write_bytes(b"picture")
            return self.answer(
                request,
                {"path": str(path), "width": 128, "height": 96, "seconds": 1.0},
            )
        return self.refuse(request, f"{method}: not answered here")

    def evaluate(self, parameters: dict[str, object]) -> dict[str, object]:
        code = str(parameters["code"])
        written = io.StringIO()
        value = ""
        try:
            with contextlib.redirect_stdout(written):
                if parameters.get("mode") == "Statements":
                    exec(compile(code, "<prompt 1>", "exec"), self.namespace)
                else:
                    result = eval(compile(code, "<prompt 1>", "eval"), self.namespace)
                    value = "" if result is None else repr(result)
        except Exception as error:
            return {
                "value": "",
                "output": written.getvalue(),
                "error": {
                    "type": type(error).__name__,
                    "message": str(error),
                    "traceback": "".join(traceback.format_exception(error)),
                },
            }
        return {"value": value, "output": written.getvalue()}

    @staticmethod
    def answer(request: dict[str, object], result: dict[str, object]) -> str:
        return json.dumps({"id": request["id"], "result": result})

    @staticmethod
    def refuse(request: dict[str, object], message: str) -> str:
        return json.dumps(
            {"id": request["id"], "error": {"code": "failed", "message": message}}
        )


class SketchConsoleTest(unittest.TestCase):
    def setUp(self) -> None:
        temporary = tempfile.TemporaryDirectory(prefix="sigil-console-")
        self.addCleanup(temporary.cleanup)
        self.root = Path(temporary.name)
        self.sketch = self.root / "hello.py"
        self.sketch.write_text("COLOR = '#efb87e'\n")
        self.host = StubHost(self.root)
        self.host.watched = self.sketch
        self.host.poll()
        self.said: list[str] = []
        self.console = SketchConsole(
            Session(self.host), sketch=self.sketch, write=self.said.append
        )

    def printed(self) -> str:
        return "".join(self.said)

    def sent(self) -> list[tuple[str, str]]:
        return [
            (request["parameters"]["code"], request["parameters"]["mode"])  # type: ignore[index]
            for request in self.host.requests
            if request["method"] == "session.evaluate"
        ]

    def test_an_expression_answers_its_value_and_a_statement_what_it_printed(self) -> None:
        self.assertFalse(self.console.push("COLOR = '#7fd1c7'"))
        self.assertFalse(self.console.push("COLOR"))
        self.assertFalse(self.console.push("print('hello', COLOR)"))
        self.assertEqual(
            self.sent(),
            [
                ("COLOR = '#7fd1c7'", "Statements"),
                ("COLOR", "Expression"),
                ("print('hello', COLOR)", "Expression"),
            ],
        )
        self.assertEqual(self.printed(), "'#7fd1c7'\nhello #7fd1c7\n")

    def test_continuation_lines_are_gathered_into_one_input(self) -> None:
        self.assertTrue(self.console.push("for step in range(2):"))
        self.assertTrue(self.console.push("    print(step)"))
        self.assertEqual(self.sent(), [])
        self.assertFalse(self.console.push(""))
        self.assertEqual(
            self.sent(), [("for step in range(2):\n    print(step)\n", "Statements")]
        )
        self.assertEqual(self.printed(), "0\n1\n")

    def test_a_raise_shows_its_traceback_and_the_prompt_goes_on(self) -> None:
        self.console.push("1 / 0")
        self.assertIn("ZeroDivisionError: division by zero", self.printed())
        self.assertIn("<prompt 1>", self.printed())
        self.said.clear()
        self.console.push("2 + 2")
        self.assertEqual(self.printed(), "4\n")

    def test_a_shut_door_is_said_by_name(self) -> None:
        self.host.door = False
        self.console.push("COLOR")
        self.assertEqual(
            self.printed(), "sigil: session.evaluate: this host evaluates no text\n"
        )

    def test_still_copies_the_hosts_picture_to_the_path_named(self) -> None:
        target = self.root / "out" / "still.png"
        self.console.push(f":still {target}")
        self.assertEqual(target.read_bytes(), b"picture")
        self.assertEqual(self.printed(), f"{target} (128 x 96)\n")
        self.assertEqual(
            [request["method"] for request in self.host.requests], ["session.still"]
        )

    def test_reload_imports_the_file_again_and_waits_for_it(self) -> None:
        os.utime(self.sketch, (0, 0))
        self.host.poll()
        self.console.push(":reload")
        self.assertGreater(self.sketch.stat().st_mtime, 0)
        self.assertEqual(self.host.imports, 3)
        self.assertEqual(self.printed(), "reloaded hello.py\n")

    def test_a_reload_the_host_never_takes_says_so(self) -> None:
        self.host.watched = None
        self.console.reload(patience=0.2)
        self.assertIn("hello.py did not load again", self.printed())

    def test_quit_ends_the_loop_and_unknown_commands_are_named(self) -> None:
        lines = iter([":nothing", "COLOR = 1", ":quit", "never read"])
        self.console.run(read=lambda _prompt: next(lines))
        self.assertTrue(self.console.finished)
        self.assertIn("sigil: no command :nothing", self.printed())
        self.assertEqual(self.sent(), [("COLOR = 1", "Statements")])

    def test_the_end_of_input_ends_the_loop(self) -> None:
        def read(_prompt: str) -> str:
            raise EOFError

        self.console.run(read=read)
        self.assertEqual(self.host.requests, [])

    def test_an_expression_is_told_from_statements(self) -> None:
        self.assertEqual(evaluationMode("COLOR"), EvaluationMode.Expression)
        self.assertEqual(evaluationMode("self.pulse()"), EvaluationMode.Expression)
        self.assertEqual(evaluationMode("COLOR = 1"), EvaluationMode.Statements)
        self.assertEqual(evaluationMode("import math"), EvaluationMode.Statements)
        self.assertEqual(evaluationMode("def"), EvaluationMode.Statements)


class PromptCommandTest(unittest.TestCase):
    def test_no_host_answering_and_no_file_says_how_to_start_one(self) -> None:
        errors = io.StringIO()
        with contextlib.redirect_stderr(errors), self.assertRaises(SystemExit) as ended:
            cli.main(["repl", "--to", "127.0.0.1:1"])
        self.assertEqual(ended.exception.code, 1)
        self.assertIn("no Grimoire answers at ws://127.0.0.1:1/sigil", errors.getvalue())
        self.assertIn("--repl", errors.getvalue())

    def test_a_file_that_is_not_python_is_refused_before_anything_starts(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            source = Path(directory) / "scene.cpp"
            source.write_text("")
            errors = io.StringIO()
            with contextlib.redirect_stderr(errors), self.assertRaises(SystemExit):
                cli.main(["repl", str(source)])
        self.assertIn("evaluates Python", errors.getvalue())


if __name__ == "__main__":
    unittest.main()
