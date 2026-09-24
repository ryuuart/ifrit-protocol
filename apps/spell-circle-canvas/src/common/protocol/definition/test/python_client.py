#!/usr/bin/env python3
"""The committed Python client, held to the C++ one.

    python_client.py --protocol-test <protocol_test binary>
    python_client.py --checker <basedpyright>

The first form has the C++ case write every table's JSON forms — the
value made with nothing set and every sample — and reads each into the
Python client: the table reads it, writes the very same JSON object back,
refuses it with a member the table does not declare as the C++ reading
does, and the value made with nothing set in Python writes the C++
default's form. Then the client itself runs over a caller of this file's
own: every method sends the definition's method name, an answer that does
not read as its result is refused with the code unreadable, and an event
that does not read as its table reaches the caller's ``refused``.

The second form type-checks the package in basedpyright's strict mode,
the mode the rest of the package is held to.
"""

from __future__ import annotations

import argparse
import collections.abc
import dataclasses
import json
import os
import pathlib
import subprocess
import sys
import tempfile
import typing
import unittest

HERE = pathlib.Path(__file__).resolve().parent
APPLICATION = HERE.parents[4]
PACKAGE_ROOT = APPLICATION.parent / "python" / "sigil"
PACKAGE = PACKAGE_ROOT / "sigil" / "protocol"
BASE = "sigil.protocol"

sys.path.insert(0, str(PACKAGE_ROOT))

from sigil import protocol
from sigil.protocol import clock, messages, registry

options: argparse.Namespace


def table_class(qualified: str) -> typing.Any:
    """The Python class the definition's table @p qualified reads as."""
    space, _, name = qualified.rpartition(".")
    module = "shared" if space == BASE else space.rpartition(".")[2]
    return getattr(getattr(protocol, module), name)


def corpus() -> dict[str, list[dict[str, typing.Any]]]:
    """Every table's JSON forms, as the C++ case wrote them."""
    with tempfile.TemporaryDirectory() as directory:
        path = pathlib.Path(directory) / "corpus.json"
        subprocess.run(
            [
                options.protocol_test,
                "--gtest_filter=ProtocolRoundTrip.EveryTableWritesTheFormsAClientReads",
            ],
            env={**os.environ, "SIGIL_PROTOCOL_CORPUS": str(path)},
            check=True,
            capture_output=True,
        )
        return json.loads(path.read_text())


class Board:
    """A caller that answers from a table of canned JSON and keeps what it
    was sent, the listeners it was handed and the refusals it heard."""

    def __init__(self, answers: dict[str, messages.Json]) -> None:
        self.answers = answers
        self.sent: list[tuple[str, dict[str, messages.Json]]] = []
        self.listeners: dict[str, collections.abc.Callable[[messages.Json], None]] = {}
        self.refusals: list[protocol.Error] = []

    def call(self, method: str, parameters: dict[str, messages.Json]) -> messages.Json:
        self.sent.append((method, parameters))
        return self.answers.get(method, {})

    def listen(
        self, method: str, listener: collections.abc.Callable[[messages.Json], None]
    ) -> None:
        self.listeners[method] = listener

    def refused(self, error: protocol.Error) -> None:
        self.refusals.append(error)


class TablesAgree(unittest.TestCase):
    """Every table reads and writes the text the C++ client does."""

    forms: typing.ClassVar[dict[str, list[dict[str, typing.Any]]]]

    @classmethod
    def setUpClass(cls) -> None:
        cls.forms = corpus()

    def test_every_table_is_a_class_of_the_client(self) -> None:
        for name in self.forms:
            with self.subTest(table=name):
                self.assertTrue(hasattr(table_class(name), "from_json"))

    def test_every_form_reads_and_writes_the_same_object(self) -> None:
        for name, forms in self.forms.items():
            for form in forms:
                with self.subTest(table=name, form=form):
                    self.assertEqual(form, table_class(name).from_json(form).to_json())

    def test_nothing_set_writes_the_c_plus_plus_default(self) -> None:
        # A table with a required member has no value made with nothing
        # set in Python, which is the definition's word for "required".
        for name, forms in self.forms.items():
            kind = table_class(name)
            if any(
                field.default is dataclasses.MISSING
                for field in dataclasses.fields(kind)
            ):
                continue
            with self.subTest(table=name):
                self.assertEqual(forms[0], kind().to_json())

    def test_a_member_the_table_does_not_declare_is_refused(self) -> None:
        for name, forms in self.forms.items():
            with self.subTest(table=name), self.assertRaises(messages.MessageError):
                table_class(name).from_json({**forms[0], "no_such_member": 1})

    def test_a_required_member_left_out_is_refused(self) -> None:
        with self.assertRaises(messages.MessageError):
            protocol.session.OpenParameters.from_json({"kind": "canvas"})


class ClientSpeaksTheDefinition(unittest.TestCase):
    """The client over a caller: the wire's names, and the client's own
    refusals."""

    def test_a_method_sends_the_definitions_name(self) -> None:
        board = Board({"clock.current": {"policy": "Advance", "frame": 120}})
        clock_client = protocol.Clock(board)
        clock_client.set_policy(policy=clock.Policy.Advance, budget_seconds=2.0)
        now = clock_client.current()
        self.assertEqual(
            [
                ("clock.setPolicy", {"policy": "Advance", "budget_seconds": 2.0}),
                ("clock.current", {}),
            ],
            board.sent,
        )
        self.assertEqual(120, now.frame)
        self.assertIs(clock.Policy.Advance, now.policy)

    def test_an_answer_that_is_no_result_is_refused_by_the_client(self) -> None:
        board = Board({"host.version": {"no_such_field": 1}})
        with self.assertRaises(protocol.ProtocolError) as raised:
            protocol.Host(board).version()
        error = raised.exception.error
        self.assertIs(protocol.ErrorCode.unreadable, error.code)
        self.assertTrue(error.message.startswith("client: host.version: "))

    def test_an_event_that_is_no_table_reaches_the_callers_refused(self) -> None:
        board = Board({})
        heard: list[registry.ChangedEvent] = []
        protocol.Registry(board).on_changed(heard.append)
        board.listeners["registry.changed"]({"names": ["hello", "cascade"]})
        self.assertEqual([("hello", "cascade")], [event.names for event in heard])
        self.assertEqual([], board.refusals)
        board.listeners["registry.changed"]({"names": 3})
        self.assertEqual(1, len(heard))
        self.assertEqual(1, len(board.refusals))
        self.assertIs(protocol.ErrorCode.unreadable, board.refusals[0].code)
        self.assertTrue(
            board.refusals[0].message.startswith("client: registry.changed: ")
        )


def type_check(checker: str) -> int:
    """The package under basedpyright's strict mode; its exit status."""
    with tempfile.TemporaryDirectory() as directory:
        configuration = pathlib.Path(directory) / "pyrightconfig.json"
        configuration.write_text(
            json.dumps(
                {
                    "include": [str(PACKAGE)],
                    "exclude": [f"{PACKAGE}/__pycache__"],
                    "extraPaths": [str(PACKAGE_ROOT)],
                    "typeCheckingMode": "strict",
                    "pythonVersion": "3.12",
                }
            )
        )
        return subprocess.run(
            [checker, "-p", str(configuration)], check=False
        ).returncode


def main() -> int:
    global options
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("--protocol-test")
    parser.add_argument("--checker")
    options, rest = parser.parse_known_args()
    if options.checker:
        return type_check(options.checker)
    if not options.protocol_test:
        parser.error("--protocol-test or --checker is required")
    program = unittest.main(argv=[sys.argv[0], *rest], exit=False)
    return 0 if program.result.wasSuccessful() else 1


if __name__ == "__main__":
    sys.exit(main())
