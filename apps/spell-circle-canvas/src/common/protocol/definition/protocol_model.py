"""The description sigil_protocol writes, read into the model both Python
writers walk: every domain with its commands and events, every table with
its fields and every enumeration with its values, each with the
documentation the definition wrote over it.

The description is the definition's reflected schema written out whole,
so nothing here re-reads the definition and nothing here decides what it
says: the names, the order and the documentation are the definition's.
"""

from __future__ import annotations

import dataclasses
import typing

BASE = "sigil.protocol"


@dataclasses.dataclass(frozen=True)
class Documented:
    documentation: tuple[str, ...]
    experimental: bool


@dataclasses.dataclass(frozen=True)
class Field(Documented):
    name: str
    type: str
    reference: str
    vector: bool
    optional: bool
    required: bool
    default: object


@dataclasses.dataclass(frozen=True)
class Table(Documented):
    name: str
    fields: tuple[Field, ...]


@dataclasses.dataclass(frozen=True)
class Value(Documented):
    name: str
    value: int


@dataclasses.dataclass(frozen=True)
class Enumeration(Documented):
    name: str
    underlying: str
    values: tuple[Value, ...]


@dataclasses.dataclass(frozen=True)
class Command(Documented):
    name: str
    method: str
    parameters: str
    result: str
    asynchronous: bool
    dispatcher: bool


@dataclasses.dataclass(frozen=True)
class Event(Documented):
    name: str
    method: str
    payload: str


@dataclasses.dataclass(frozen=True)
class Domain(Documented):
    name: str
    space: str
    service: str
    events: Documented
    commands: tuple[Command, ...]
    event_list: tuple[Event, ...]


@dataclasses.dataclass(frozen=True)
class Model:
    empty: str
    domains: tuple[Domain, ...]
    tables: tuple[Table, ...]
    enumerations: tuple[Enumeration, ...]

    def table(self, name: str) -> Table:
        for table in self.tables:
            if table.name == name:
                return table
        raise KeyError(name)

    def enumeration(self, name: str) -> Enumeration:
        for enumeration in self.enumerations:
            if enumeration.name == name:
                return enumeration
        raise KeyError(name)

    def tables_in(self, space: str) -> tuple[Table, ...]:
        return tuple(table for table in self.tables if space_of(table.name) == space)

    def enumerations_in(self, space: str) -> tuple[Enumeration, ...]:
        return tuple(
            enumeration
            for enumeration in self.enumerations
            if space_of(enumeration.name) == space
        )


def space_of(qualified: str) -> str:
    """Everything before the last dotted word."""
    return qualified.rpartition(".")[0]


def last_word(qualified: str) -> str:
    return qualified.rpartition(".")[2]


def _documented(raw: dict[str, typing.Any]) -> dict[str, typing.Any]:
    return {
        "documentation": tuple(raw["documentation"]),
        "experimental": bool(raw["experimental"]),
    }


def read(description: dict[str, typing.Any]) -> Model:
    """The model the description holds."""
    domains = tuple(
        Domain(
            **_documented(domain),
            name=domain["name"],
            space=domain["space"],
            service=domain["service"],
            events=Documented(**_documented(domain["events"])),
            commands=tuple(
                Command(
                    **_documented(command),
                    name=command["name"],
                    method=command["method"],
                    parameters=command["parameters"],
                    result=command["result"],
                    asynchronous=command["asynchronous"],
                    dispatcher=command["dispatcher"],
                )
                for command in domain["commands"]
            ),
            event_list=tuple(
                Event(
                    **_documented(event),
                    name=event["name"],
                    method=event["method"],
                    payload=event["payload"],
                )
                for event in domain["eventList"]
            ),
        )
        for domain in description["domains"]
    )
    tables = tuple(
        Table(
            **_documented(table),
            name=table["name"],
            fields=tuple(
                Field(
                    **_documented(field),
                    name=field["name"],
                    type=field["type"],
                    reference=field["reference"],
                    vector=field["vector"],
                    optional=field["optional"],
                    required=field["required"],
                    default=field["default"],
                )
                for field in table["fields"]
            ),
        )
        for table in description["tables"]
    )
    enumerations = tuple(
        Enumeration(
            **_documented(enumeration),
            name=enumeration["name"],
            underlying=enumeration["underlying"],
            values=tuple(
                Value(**_documented(value), name=value["name"], value=value["value"])
                for value in enumeration["values"]
            ),
        )
        for enumeration in description["enumerations"]
    )
    return Model(description["empty"], domains, tables, enumerations)
