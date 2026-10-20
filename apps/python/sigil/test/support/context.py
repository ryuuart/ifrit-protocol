"""A context that records what a sketch's hooks declare on it, and the
module a test's sketch classes are decorated into."""

import sys
from types import ModuleType

from sigil.sketch import sketch


class RecordingContext:
    """What a hook's context answers, recording what is declared on it."""

    elapsed = 0.0

    def __init__(self, width=960, height=640):
        self.width, self.height = width, height
        self.calls = []

    def canvas(self, *size):
        if len(size) == 2:
            self.width, self.height = size
        self.calls.append(("canvas", size))

    def background(self, color):
        self.calls.append(("background", color))

    def captureAt(self, seconds):
        self.calls.append(("capture", seconds))

    def render(self, element):
        self.calls.append(("render", element))


class DecoratedModule:
    """A test case mixin: a module of its own, registered for the length of
    one case, that `decorate` files a sketch class into."""

    moduleName = "_sigil_test_fixture"

    def setUp(self):
        super().setUp()
        self.module = ModuleType(self.moduleName)
        sys.modules[self.module.__name__] = self.module
        self.addCleanup(sys.modules.pop, self.module.__name__, None)

    def decorate(self, cls, **options):
        cls.__module__ = self.module.__name__
        return sketch(**options)(cls)
