"""Selectors: which elements a rule speaks about, and the sheet applying it.

A selector is CSS's grammar over the element tree as one comparable
value, and the two front doors onto it — the CSS text and the typed
builders under `compose.select` — must produce the same value. The cases
below ask the value everything that needs no canvas: that the two doors
agree, that the three operators spell the three pseudo-classes, what a
selector weighs and how weights order, that nothing absorbs, what a rule
states and reads back, and how sheets join and include one another. They
then go to a session for what only pixels can say: that a rule reaches
the elements its selector names, that a combinator and a structural
pseudo-class narrow it, that a sheet sees only the subtree it was
applied to, and that the node's own verb still wins.
"""

import tempfile
import unittest
from pathlib import Path

from _sigil import compose as native
from sigil import compose, image, material, weave
from sigil.compose import select
from sigil.sketch import render_file

ElementSelector = compose.ElementSelector
Rule = compose.Rule
Specificity = compose.Specificity
StyleSheet = compose.StyleSheet

# The weight table of the selector chapter, as CSS counts it where
# nothing carries an id.
WEIGHTS = (
    ("*", 0, 0),
    ("heading", 0, 1),
    (".card", 1, 0),
    (".card > heading", 1, 1),
    (".row:nth-child(2n+1)", 2, 0),
    (":is(.a, heading)", 1, 0),
    (":where(.a, .b.c)", 0, 0),
)


class Spellings(unittest.TestCase):
    def test_the_package_names_what_the_extension_registers(self):
        for name in ("ElementSelector", "Specificity", "Rule", "StyleSheet"):
            with self.subTest(name=name):
                self.assertIs(getattr(compose, name), getattr(native, name))
        self.assertIs(compose.selector, native.selector)
        self.assertIs(compose.rule, native.rule)
        self.assertIs(select.styleClass, native.select.styleClass)

    def test_each_name_reports_the_module_an_author_imports(self):
        for value in (ElementSelector, Specificity, Rule, StyleSheet):
            with self.subTest(value=value.__qualname__):
                self.assertEqual(value.__module__, "sigil.compose")
        self.assertEqual(select.is_.__module__, "sigil.compose.select")
        self.assertEqual(compose.selector.__module__, "sigil.compose")

    def test_the_selector_sheet_is_not_the_name_keyed_text_sheet(self):
        # Two libraries carry a StyleSheet and a Rule under one word. The
        # Compose pair is keyed by selectors and put in force with
        # applyStyleSheet; the Weave pair is keyed by class names and
        # stated with styleSheet.
        self.assertIsNot(compose.StyleSheet, weave.StyleSheet)
        self.assertIsNot(compose.Rule, weave.Rule)
        self.assertTrue(hasattr(compose.Element, "applyStyleSheet"))
        self.assertTrue(hasattr(compose.Element, "styleSheet"))


class Grammar(unittest.TestCase):
    def test_the_css_text_and_the_typed_builders_agree(self):
        cases = (
            (".card > .title:first-child",
             select.styleClass("card").child(
                 select.styleClass("title").firstChild())),
            (".card .title", select.styleClass("card").descendant(
                select.styleClass("title"))),
            (".a + .b", select.styleClass("a").next(select.styleClass("b"))),
            (".a ~ .b", select.styleClass("a").sibling(select.styleClass("b"))),
            ("heading", select.role("heading")),
            ("*", select.any()),
            (".card.wide", select.styleClass("card") & select.styleClass("wide")),
            (".a, heading", select.styleClass("a") | select.role("heading")),
            (":not(.x)", ~select.styleClass("x")),
            (":is(.a, .b) .c", select.is_(
                select.styleClass("a") | select.styleClass("b")).descendant(
                    select.styleClass("c"))),
            (":where(.a)", select.where(select.styleClass("a"))),
            (".row:nth-child(odd)", select.styleClass("row").nthChild(2, 1)),
            (".row:nth-child(even)", select.styleClass("row").nthChild(2, 0)),
            (".row:nth-last-child(2n+1)",
             select.styleClass("row").nthLastChild(2, 1)),
            (".row:nth-child(2n+1 of .x)", select.styleClass("row").nthChild(
                2, 1, of=select.styleClass("x"))),
            (".row:last-child", select.styleClass("row").lastChild()),
            (".row:only-child", select.styleClass("row").onlyChild()),
            ("heading:first-of-type", select.role("heading").firstOfType()),
            ("heading:last-of-type", select.role("heading").lastOfType()),
            ("heading:only-of-type", select.role("heading").onlyOfType()),
            ("heading:nth-of-type(2n)", select.role("heading").nthOfType(2, 0)),
            ("heading:nth-last-of-type(2n)",
             select.role("heading").nthLastOfType(2, 0)),
            (".card:empty", select.styleClass("card").empty()),
            (".card:root", select.styleClass("card").root()),
        )
        for text, built in cases:
            with self.subTest(text=text):
                self.assertEqual(compose.selector(text), built)
                self.assertFalse(built.matchesNothing())

    def test_notAnyOf_and_the_tilde_spell_one_negation(self):
        self.assertEqual(
            select.notAnyOf(select.styleClass("x")), ~select.styleClass("x")
        )
        self.assertEqual(
            select.is_(select.styleClass("a") | select.styleClass("b")),
            compose.selector(":is(.a, .b)"),
        )

    def test_a_list_is_flat_and_a_list_compounded_is_is(self):
        a, b, c = (select.styleClass(name) for name in "abc")
        self.assertEqual((a | b) | c, a | (b | c))
        self.assertEqual((a | b) & c, compose.selector(":is(.a, .b).c"))

    def test_a_selector_this_library_does_not_read_matches_nothing(self):
        for text in ("#id", "[data-x]", ".a:hover", "::before"):
            with self.subTest(text=text):
                self.assertTrue(compose.selector(text).matchesNothing())
        self.assertTrue(ElementSelector().matchesNothing())

    def test_nothing_is_the_zero_of_the_algebra_but_the_identity_of_a_list(self):
        nothing = ElementSelector()
        card = select.styleClass("card")
        for narrowed in (
            nothing & card,
            card & nothing,
            ~nothing,
            nothing.firstChild(),
            nothing.child(card),
            card.child(nothing),
        ):
            self.assertTrue(narrowed.matchesNothing())
        self.assertEqual(nothing | card, card)
        self.assertEqual(card | nothing, card)


class Weight(unittest.TestCase):
    def test_a_selector_weighs_what_css_counts(self):
        for text, classes, roles in WEIGHTS:
            with self.subTest(text=text):
                self.assertEqual(
                    compose.selector(text).specificity(),
                    Specificity(classes=classes, roles=roles),
                )

    def test_one_class_outweighs_any_number_of_roles(self):
        ordered = [
            compose.selector("*").specificity(),
            compose.selector("heading").specificity(),
            compose.selector("a b c heading").specificity(),
            compose.selector(".card").specificity(),
            compose.selector(".card heading").specificity(),
            compose.selector(".card.wide").specificity(),
        ]
        self.assertEqual(ordered, sorted(ordered))
        self.assertLess(ordered[2], ordered[3])
        self.assertGreater(ordered[3], ordered[1])
        self.assertLessEqual(ordered[0], ordered[0])
        self.assertGreaterEqual(ordered[0], ordered[0])

    def test_a_weight_is_a_value_that_hashes_with_its_equality(self):
        self.assertEqual(Specificity(1, 2), Specificity(classes=1, roles=2))
        self.assertNotEqual(Specificity(1, 2), Specificity(2, 1))
        self.assertEqual(hash(Specificity(1, 2)), hash(Specificity(1, 2)))
        self.assertEqual(len({Specificity(1, 2), Specificity(1, 2)}), 1)

    def test_a_selector_and_what_holds_one_carry_no_hash(self):
        # Equality is structural over a shared body the bindings cannot
        # read back, so nothing here can key a dictionary.
        for value in (
            select.styleClass("card"),
            compose.rule(".card"),
            StyleSheet([compose.rule(".card")]),
        ):
            with self.subTest(value=repr(value)):
                with self.assertRaises(TypeError):
                    hash(value)

    def test_a_repr_says_what_a_selector_is(self):
        self.assertIn("classes=1", repr(select.styleClass("card")))
        self.assertIn("nothing", repr(ElementSelector()))
        self.assertIn("2 rules", repr(
            StyleSheet([compose.rule(".a"), compose.rule(".b")])))


class Rules(unittest.TestCase):
    def test_both_front_doors_build_one_rule(self):
        self.assertEqual(
            compose.rule(".card"), compose.rule(select.styleClass("card"))
        )
        self.assertEqual(compose.rule(".card"), Rule(select.styleClass("card")))
        self.assertEqual(
            compose.rule(".card").selector(), compose.selector(".card")
        )

    def test_a_rule_folds_the_partials_a_verb_writes(self):
        stated = compose.rule(".card").font(weave.Type(size=18))
        stated.font(weave.Type(color="#ff0000"))
        self.assertEqual(stated.type().size.value, 18)
        self.assertEqual(stated.type().color, material.Color("#ff0000"))
        stated.block(weave.Block(widowLines=3))
        self.assertEqual(stated.block().widowLines, 3)
        self.assertFalse(stated.block().empty())

    def test_a_rule_states_an_ink_and_a_custom_property(self):
        coloured = compose.rule(".card").ink("#00ff00")
        referenced = compose.rule(".card").ink(compose.var("accent"))
        self.assertNotEqual(coloured, referenced)
        self.assertEqual(
            compose.rule(".card").var("gutter", 24.0),
            compose.rule(".card").var("gutter", 24.0),
        )
        self.assertNotEqual(
            compose.rule(".card").var("tint", "#00ff00"),
            compose.rule(".card").var("tint", 24.0),
        )

    def test_a_rule_compares_by_what_it_says_and_who_it_says_it_to(self):
        self.assertEqual(
            compose.rule(".card").font(weave.Type(size=18)),
            compose.rule(select.styleClass("card")).font(weave.Type(size=18)),
        )
        self.assertNotEqual(
            compose.rule(".card").font(weave.Type(size=18)),
            compose.rule(".card").font(weave.Type(size=19)),
        )


class Sheets(unittest.TestCase):
    def test_a_sheet_holds_its_rules_in_order(self):
        first, second = compose.rule(".a"), compose.rule(".b")
        sheet = StyleSheet([first, second])
        self.assertEqual(sheet.rules(), [first, second])
        self.assertEqual(len(sheet), 2)
        self.assertEqual(sheet.size(), 2)
        self.assertEqual(list(sheet), [first, second])
        self.assertTrue(StyleSheet().empty())
        self.assertFalse(sheet.empty())

    def test_the_rules_read_back_are_the_callers_own_list(self):
        sheet = StyleSheet([compose.rule(".a")])
        held = sheet.rules()
        held.append(compose.rule(".b"))
        self.assertEqual(len(sheet.rules()), 1)

    def test_a_sheet_joins_and_includes_another_in_order(self):
        house = StyleSheet([compose.rule(".a")])
        local = StyleSheet([compose.rule(".b"), compose.rule(".c")])
        self.assertEqual((house + local).rules(), house.rules() + local.rules())
        self.assertEqual(house + local, StyleSheet([house, local]))
        self.assertEqual(
            StyleSheet([house, compose.rule(".b"), compose.rule(".c")]),
            house + local,
        )
        self.assertEqual(len(house + local), 3)

    def test_a_statement_is_a_rule_or_a_sheet(self):
        with self.assertRaises(TypeError):
            StyleSheet([".a"])
        self.assertEqual(StyleSheet(), StyleSheet([]))
        self.assertEqual(
            StyleSheet(rule for rule in (compose.rule(".a"),)),
            StyleSheet([compose.rule(".a")]),
        )


class Matching(unittest.TestCase):
    """What only a resolved tree can answer, read off the pixels."""

    def render(self, body, *, width, height):
        with tempfile.TemporaryDirectory() as folder:
            source = Path(folder) / "scene.py"
            output = Path(folder) / "scene.png"
            source.write_text(body)
            render_file(source, output, at=0)
            pixels = image.load(output).rgba()
        return [
            self.cell(pixels, width, height, start, span)
            for start, span in self.cells
        ]

    @staticmethod
    def cell(pixels, width, height, start, span):
        return b"".join(
            pixels[(y * width + start) * 4 : (y * width + start + span) * 4]
            for y in range(height)
        )

    def test_a_class_rule_restyles_text_and_the_nodes_own_verb_still_wins(self):
        self.cells = ((0, 64), (64, 64), (128, 64), (192, 64))
        matched, reference, overridden, override = self.render(
            """from sigil.compose import StyleSheet, box, rule, text
from sigil.sketch import sketch
from sigil.weave import Type


@sketch(size=(256, 48), background="#000000")
class Scene:
    def setup(self, ctx):
        pass

    def update(self, elapsed, ctx):
        sheet = StyleSheet([rule(".title").font(Type(size=30, color="#ff0000"))])
        cells = [
            text("HI").styleClass("title"),
            text("HI").font(Type(size=30, color="#ff0000")),
            text("HI").styleClass("title").font(Type(size=14)),
            text("HI").font(Type(size=14, color="#ff0000")),
        ]
        ctx.render(
            box()
            .applyStyleSheet(sheet)
            .children(
                box().width(64).height(48).children([cell]) for cell in cells
            )
            .row()
        )
""",
            width=256,
            height=48,
        )
        self.assertTrue(any(matched[::4]))
        self.assertEqual(matched, reference)
        self.assertEqual(overridden, override)
        self.assertNotEqual(matched, overridden)

    def test_a_child_combinator_and_a_sheet_that_sees_only_its_subtree(self):
        # The same rule, `.page > .swatch`, applied at the `.page` node
        # and applied below it. The applying node is inside its own
        # sheet, so the first matches; nothing above an applying node
        # can satisfy any part of a rule, so the second does not.
        self.cells = ((0, 64), (64, 64), (128, 64), (192, 64))
        atThePage, belowThePage, styled, plain = self.render(
            """from sigil.compose import StyleSheet, box, rule, text
from sigil.sketch import sketch
from sigil.weave import Type


@sketch(size=(256, 48), background="#000000")
class Scene:
    def setup(self, ctx):
        pass

    def update(self, elapsed, ctx):
        sheet = StyleSheet(
            [rule(".page > .swatch").font(Type(size=30, color="#ff0000"))]
        )

        def cell():
            return box().width(64).height(48)

        ctx.render(
            box()
            .children(
                [
                    cell()
                    .styleClass("page")
                    .applyStyleSheet(sheet)
                    .children([box().styleClass("swatch").children([text("HI")])]),
                    cell()
                    .styleClass("page")
                    .children(
                        [
                            box()
                            .styleClass("swatch")
                            .applyStyleSheet(sheet)
                            .children([text("HI")])
                        ]
                    ),
                    cell().children(
                        [
                            box()
                            .font(Type(size=30, color="#ff0000"))
                            .children([text("HI")])
                        ]
                    ),
                    cell().children([box().children([text("HI")])]),
                ]
            )
            .row()
        )
""",
            width=256,
            height=48,
        )
        self.assertTrue(any(atThePage[::4]))
        self.assertEqual(atThePage, styled)
        self.assertEqual(belowThePage, plain)
        self.assertNotEqual(atThePage, belowThePage)

    def test_a_structural_pseudo_class_narrows_a_rule_to_one_sibling(self):
        # `.item:first-child` reaches the first of two siblings and not
        # the second, and what it states inherits into the text below.
        self.cells = ((0, 128), (128, 128))
        selected, reference = self.render(
            """from sigil.compose import StyleSheet, box, rule, text
from sigil.sketch import sketch
from sigil.weave import Type


@sketch(size=(256, 48), background="#000000")
class Scene:
    def setup(self, ctx):
        pass

    def update(self, elapsed, ctx):
        sheet = StyleSheet(
            [rule(".item:first-child").font(Type(size=30, color="#ff0000"))]
        )

        def half():
            return box().width(64).height(48)

        ctx.render(
            box()
            .children(
                [
                    box()
                    .width(128)
                    .height(48)
                    .applyStyleSheet(sheet)
                    .children(
                        [
                            half().styleClass("item").children([text("HI")]),
                            half().styleClass("item").children([text("HI")]),
                        ]
                    )
                    .row(),
                    box()
                    .width(128)
                    .height(48)
                    .children(
                        [
                            half()
                            .font(Type(size=30, color="#ff0000"))
                            .children([text("HI")]),
                            half().children([text("HI")]),
                        ]
                    )
                    .row(),
                ]
            )
            .row()
        )
""",
            width=256,
            height=48,
        )
        self.assertTrue(any(selected[::4]))
        self.assertEqual(selected, reference)


if __name__ == "__main__":
    unittest.main()
