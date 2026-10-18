"""Every public name in the package is spelled as its C++ twin is.

The package is camelCase throughout, one to one with the native
libraries; a name a Python keyword collides with takes a trailing
underscore. This walks every public module the package ships and every
class it defines, so a snake_case name cannot return anywhere — the
protocol client included, whose wire members keep the definition's own
spelling while their attributes are camelCase.
"""

import dataclasses
import importlib
import inspect
import pkgutil
import re
import unittest

import sigil

SNAKE = re.compile(r"^[a-z][a-z0-9]*(?:_[a-z0-9]+)+$")


def public_modules():
    for info in pkgutil.walk_packages(sigil.__path__, "sigil."):
        parts = info.name.split(".")
        if any(part.startswith("_") for part in parts) or "examples" in parts:
            continue
        yield importlib.import_module(info.name)


def defined_here(value, module):
    owner = getattr(value, "__module__", None)
    return owner is None or owner == module.__name__ or owner.startswith("sigil.")


def own_names(cls):
    """The names a class of the package defines itself, fields included,
    leaving out what it inherits from Python's own classes."""
    names = set()
    for base in inspect.getmro(cls):
        if base.__module__ in ("builtins", "enum", "typing", "abc"):
            continue
        names.update(vars(base))
    if dataclasses.is_dataclass(cls):
        names.update(field.name for field in dataclasses.fields(cls))
    return {name for name in names if not name.startswith("_")}


class Naming(unittest.TestCase):
    def test_no_public_name_is_snake_case(self):
        offenders = []
        for module in public_modules():
            for name in dir(module):
                if name.startswith("_"):
                    continue
                value = getattr(module, name)
                if not defined_here(value, module):
                    continue
                if SNAKE.match(name):
                    offenders.append(f"{module.__name__}.{name}")
                if inspect.isclass(value):
                    offenders += [
                        f"{module.__name__}.{name}.{attribute}"
                        for attribute in sorted(own_names(value))
                        if SNAKE.match(attribute)
                    ]
        self.assertEqual(sorted(set(offenders)), [])

    def test_the_protocol_client_is_camel_case_over_the_wire_spelling(self):
        from sigil.protocol import Clock, clock

        self.assertTrue(callable(Clock.setPolicy))
        self.assertTrue(callable(Clock.onBudgetExpired))
        self.assertFalse(hasattr(Clock, "set_policy"))
        parameters = clock.SetPolicyParameters(budgetSeconds=2.5)
        self.assertEqual(parameters.toJson()["budget_seconds"], 2.5)
        self.assertEqual(
            clock.SetPolicyParameters.fromJson(parameters.toJson()), parameters
        )


if __name__ == "__main__":
    unittest.main()
