"""The copy protocol every bound record answers, and the memo models it allows."""

import builtins
import copy
import tempfile
import unittest
from pathlib import Path

from sigil import material
from sigil.compose import kit as neutral
from sigil.sketch import kit, render_file


class ValueCopyProtocol(unittest.TestCase):
    def render(self, source, at=0.08):
        with tempfile.TemporaryDirectory() as folder:
            entry = Path(folder) / "copy_protocol.py"
            output = Path(folder) / "copy_protocol.png"
            entry.write_text(source)
            render_file(entry, output, at=at)
            self.assertTrue(output.is_file())

    def setUp(self):
        builtins._sigil_copy_protocol = []
        self.addCleanup(lambda: builtins.__dict__.pop("_sigil_copy_protocol", None))

    def test_every_copy_of_a_record_equals_the_original_and_stands_apart(self):
        # The records that declare equality natively; a record without it
        # is compared field by field further down.
        for original in (
            kit.house_theme(),
            kit.Palette(ground="#112233", ink="#eeeeee"),
            kit.Register(size=18, mono=True),
        ):
            with self.subTest(record=type(original).__name__):
                for made in (
                    original.copy(),
                    copy.copy(original),
                    copy.deepcopy(original),
                ):
                    self.assertIsInstance(made, type(original))
                    self.assertEqual(made, original)
                    self.assertIsNot(made, original)

    def test_a_deep_copy_of_a_theme_does_not_follow_the_original(self):
        original = kit.house_theme()
        made = copy.deepcopy(original)
        made.type.title.size += 3
        made.palette.ground = "#244466"
        self.assertNotEqual(made, original)
        self.assertEqual(original, kit.house_theme())

    def test_a_deep_copy_of_a_colour_is_a_colour_of_its_own(self):
        original = material.Color(0.25, 0.5, 0.75, 1)
        made = copy.deepcopy(original)
        self.assertEqual(made, original)
        made.g = 0.125
        self.assertEqual(original.g, 0.5)
        self.assertEqual(copy.copy(original), original)

    def test_a_deep_copy_keeps_the_fields_a_record_was_constructed_with(self):
        original = neutral.Well(width=120, padding=8, corners=4, clip=False)
        made = copy.deepcopy(original)
        self.assertEqual(made.width, original.width)
        self.assertEqual(made.padding, 8)
        self.assertEqual(made.corners, 4)
        self.assertFalse(made.clip)
        made.padding = 16
        self.assertEqual(original.padding, 8)

    def test_a_deep_copy_shares_the_python_part_a_record_retains(self):
        calls = builtins._sigil_copy_protocol

        def label(words):
            calls.append(words)
            return neutral.caption_label(words)

        original = neutral.Caption(label=label)
        made = copy.deepcopy(original)
        original.label("first", original)
        made.label("second", made)
        self.assertEqual(calls, ["first", "second"])

    def test_the_memo_dictionary_keeps_one_copy_per_aliased_record(self):
        original = kit.house_theme()
        first, second = copy.deepcopy([original, original])
        self.assertIs(first, second)
        self.assertEqual(first, original)
        # The protocol reads nothing out of the memo, so a dictionary
        # holding unrelated entries changes nothing about the answer.
        self.assertEqual(original.__deepcopy__({id(original): "ignored"}), original)
        self.assertEqual(original.__deepcopy__(memo={}), original)

    def test_a_memo_model_holding_a_theme_prunes_and_rebuilds(self):
        self.render("""import builtins
from sigil.compose import box, memo
from sigil.sketch import kit, sketch


@sketch(size=(32, 32), background="#000000")
class Scene:
    def setup(self, ctx):
        self.frame = 0

    def update(self, elapsed, ctx):
        self.frame += 1
        look = kit.house_theme()
        if self.frame >= 3:
            look.type.title.size = 40

        def describe(model):
            builtins._sigil_copy_protocol.append(model.type.title.size)
            return box().width(32).height(32).fill(model.palette.ground)

        ctx.render((memo(look, describe).key("subject")))
""")
        sizes = builtins._sigil_copy_protocol
        self.assertEqual(len(sizes), 2)
        self.assertNotEqual(sizes[0], 40)
        self.assertEqual(sizes[1], 40)

    def test_a_deep_copy_taken_inside_a_sketch_outlives_the_session(self):
        self.render(
            """import builtins
import copy
from sigil.compose import box
from sigil.sketch import kit, sketch


@sketch(size=(16, 16), capture_at=0)
class Scene:
    def setup(self, ctx):
        look = kit.house_theme()
        look.palette.ground = "#112233"
        builtins._sigil_copy_protocol.append(copy.deepcopy(look))
        ctx.render(box().width(16).height(16))
""",
            at=0,
        )
        (held,) = builtins._sigil_copy_protocol
        self.assertIsInstance(held, kit.Theme)
        self.assertEqual(held.palette.ground, material.Color("#112233"))
        expected = kit.house_theme()
        expected.palette.ground = "#112233"
        self.assertEqual(held, expected)


if __name__ == "__main__":
    unittest.main()
