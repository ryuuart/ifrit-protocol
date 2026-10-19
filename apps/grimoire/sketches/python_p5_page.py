"""A page written the way HTML writes one: a function that returns the tree.

The file has no class and no decorator. describe(seconds) returns the page
at a scene time, and the host draws it again at every step, patching only
what changed.

EDIT THESE FIRST
  STEPS — the list the page reads out.
  describe() — the tree itself.

TAGS: Runtime/Starter, Typography/Documents
"""

from sigil.compose import Element, box, document
from sigil.sketch import kit
from sigil.sketch.globals import *

STEPS = [
    "Write a function that returns a tree.",
    "Declare the canvas with size() and a theme().",
    "Save the file; the page redraws.",
]

size(900, 460)
theme(kit.featureTheme(kit.Density.Spacious))
captureAt(1.0)


def describe(seconds: float) -> Element:
    filled = min(seconds / 1.5, 1.0)
    return kit.page(
        document.article(
            document.h2("A page is a function of time."),
            document.paragraph(
                "Like a document in a browser, the tree is the whole program: "
                "boxes, text and lists, laid out and drawn natively."
            ),
            document.list(*(document.item(step) for step in STEPS)),
            box(box().width(f"{filled * 100}%").height(8).fill("#8bd0bd"))
            .height(8)
            .borderRadius(4)
            .fill("#ffffff22")
            .overflow("clip"),
        ).gap(14),
        title="Hello, page.",
        subtitle="describe(seconds) returns the page at a scene time.",
    )
