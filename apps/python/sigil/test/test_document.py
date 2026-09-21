"""Native document roles, child vocabulary and tree-scoped styling."""

import tempfile
import unittest
from pathlib import Path

from _sigil import compose as raw
from sigil import image
from sigil.compose import Element, Text, box
from sigil.compose import document as doc
from sigil.sketch import render_file
from sigil.weave import Type, em, rich, rule


class Document(unittest.TestCase):
    def render(self, source):
        with tempfile.TemporaryDirectory() as folder:
            entry, output = Path(folder) / "document.py", Path(folder) / "document.png"
            entry.write_text(source)
            render_file(entry, output, at=0)
            return image.load(output).rgba()

    def test_factories_are_native_and_named_arguments_remain_available(self):
        self.assertIs(doc.article, raw.document.article)
        for element in (
            doc.heading(level=2, words="Heading"),
            *(
                heading(words="Heading")
                for heading in (doc.h1, doc.h2, doc.h3, doc.h4, doc.h5, doc.h6)
            ),
            doc.paragraph(words=rich().add("A ").add("passage", Type(weight=600))),
            doc.lead(words="Introduction"),
            doc.caption(words="Supporting note"),
            doc.label(words="Label"),
            doc.eyebrow(words="Series"),
            doc.footer(words="Source"),
            doc.code(words="print('Hello')"),
            doc.item(words="First", marker="1."),
            doc.item(body=doc.paragraph("Second"), marker="2."),
            doc.figure(body=box(), note="A figure"),
            doc.quote(words="A quotation"),
            doc.rule(),
        ):
            # The prose factories hand back the text leaf they make; the
            # ones that wrap it in a row or a column hand back a node.
            self.assertIsInstance(element, (Element, Text))
        self.assertEqual(doc.list_gap, raw.document.listGap)
        self.assertEqual(doc.quote_inset, raw.document.quoteInset)
        for level in (0, 7):
            with self.assertRaises(IndexError):
                doc.heading(level, "Outside the hierarchy")

    def test_containers_share_variadic_and_iterable_children(self):
        for factory in (doc.article, doc.section, doc.list, doc.quote):
            with self.subTest(factory=factory.__name__):
                first, second = doc.paragraph("One"), doc.paragraph("Two")
                for values in ([first, second], (first, second), iter((first, second))):
                    self.assertIsInstance(factory(values), Element)
                self.assertIsInstance(factory(first, second), Element)
                self.assertIsInstance(factory(), Element)
                with self.assertRaises(TypeError):
                    factory((first, None))
                with self.assertRaises(TypeError):
                    factory(first, (second,))
        with self.assertRaises(TypeError):
            doc.article("Unclassified text")
        node = doc.article()
        self.assertIs(node.children(doc.paragraph("Appended")), node)

    def test_role_and_default_properties_are_fluent_native_declarations(self):
        element = box()
        self.assertIs(element.role("notice"), element)
        self.assertIs(element.role(rule("notice").font(Type(weight=600))), element)
        self.assertIs(
            element.varDefaults({doc.measure: em(30), "accent": "#123456"}), element
        )
        with self.assertRaises(TypeError):
            element.varDefaults({doc.measure: object()})

    def test_late_role_styles_classes_and_direct_overrides_render(self):
        pixels = self.render("""from sigil.compose import box, row
from sigil.compose import document as doc
from sigil.sketch import sketch
from sigil.weave import StyleSheet, Type, rule

@sketch(size=(256, 64), background="#000000")
class Document:
    def setup(self, ctx):
        children = [doc.h2("HI"), doc.h2("HI").styleClass("special"),
                    doc.h2("HI").ink("#00ff00"), doc.paragraph("HI")]
        theme = StyleSheet([rule("h2").font(Type(size=30, color="#ff0000")),
                            rule("special").font(Type(color="#0000ff")),
                            rule("paragraph").font(Type(size=30, color="#ffffff"))])
        ctx.render(row(box(child).width(64).height(64) for child in children)
                   .styleSheet(theme))
""")
        for index, expected in enumerate(
            (
                b"\xff\x00\x00\xff",
                b"\x00\x00\xff\xff",
                b"\x00\xff\x00\xff",
                b"\xff\xff\xff\xff",
            )
        ):
            region = b"".join(
                pixels[(y * 256 + index * 64) * 4 : (y * 256 + (index + 1) * 64) * 4]
                for y in range(64)
            )
            colors = {
                region[offset : offset + 4] for offset in range(0, len(region), 4)
            }
            self.assertIn(expected, colors)


if __name__ == "__main__":
    unittest.main()
