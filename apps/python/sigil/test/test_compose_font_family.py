"""CSS's family list: a run is set in the first family of the list the
host's font context has, the generic names are the platform's families,
and an element, a rule and a span take the one string alike."""

import tempfile
import unittest
from pathlib import Path

from sigil import image
from sigil.sketch import render_file

SCENE = """from sigil.compose import SpanStyle, StyleSheet, box, rule, text
from sigil.sketch import sketch
from sigil.weave import selectors

WORDS = "Hamburgefonstiv"
FAMILY = {family!r}
WHERE = {where!r}


def words():
    return text(WORDS).fontSize(28).ink("#ffffff")


@sketch(size=(320, 48), background="#000000", capture_at=0)
class Scene:
    def setup(self, ctx):
        if WHERE == "element":
            leaf = words().fontFamily(FAMILY)
            ctx.render(box().children(leaf))
        elif WHERE == "rule":
            sheet = StyleSheet([rule(".t").fontFamily(FAMILY)])
            ctx.render(box().applyStyleSheet(sheet).children(words().styleClass("t")))
        else:
            every = selectors.range(0, len(WORDS))
            ctx.render(box().children(words().span(every, SpanStyle().fontFamily(FAMILY))))
"""


class FontFamilyList(unittest.TestCase):
    def render(self, family, where="element"):
        with tempfile.TemporaryDirectory() as folder:
            source = Path(folder) / "scene.py"
            output = Path(folder) / "scene.png"
            source.write_text(SCENE.format(family=family, where=where))
            render_file(source, output, at=0)
            return image.load(output).rgba()

    def test_a_list_is_set_in_its_first_installed_family(self):
        georgia = self.render("Georgia")
        self.assertEqual(
            self.render("Nobody Installed This Family, Georgia, Impact"), georgia
        )
        self.assertNotEqual(self.render("Impact"), georgia)

    def test_a_generic_name_is_the_platforms_family(self):
        serif = self.render("serif")
        self.assertEqual(self.render("Nobody Installed This Family, serif"), serif)
        self.assertNotEqual(self.render("monospace"), serif)

    def test_a_rule_and_a_span_take_the_same_string(self):
        family = "Nobody Installed This Family, Georgia"
        georgia = self.render("Georgia")
        self.assertEqual(self.render(family, "rule"), georgia)
        self.assertEqual(self.render(family, "span"), georgia)


if __name__ == "__main__":
    unittest.main()
