"""A face is `weave.Face` in Python, as `weave::Face` is in C++.

The house kit answers one, a style takes one, and it is read from and
handed to wherever a Skia typeface is taken, because the two hold one
typeface shared; None is no face, the default family.
"""

import unittest

from sigil import skia, weave
from sigil.sketch import kit


class Faces(unittest.TestCase):
    def test_the_house_face_is_a_weave_face(self):
        book = kit.houseFace(kit.Voice.Book)
        self.assertIsInstance(book, weave.Face)
        self.assertEqual(book, kit.houseFace(kit.Voice.Book))
        self.assertEqual(hash(book), hash(kit.houseFace(kit.Voice.Book)))
        self.assertTrue(book.familyName())
        self.assertEqual(book.style().weight, 400)

    def test_a_slant_is_asked_for_by_name_or_by_italic(self):
        slanted = kit.houseFace(kit.Voice.Book, 400, weave.FaceSlant.Italic)
        self.assertEqual(slanted, kit.houseFace(kit.Voice.Book, italic=True))
        self.assertEqual(
            kit.houseFace(kit.Voice.Book),
            kit.houseFace(kit.Voice.Book, slant=weave.FaceSlant.Upright),
        )

    def test_a_face_and_a_typeface_stand_for_each_other(self):
        face = kit.houseFace(kit.Voice.Interface)
        typeface = face.typeface()
        self.assertIsInstance(typeface, skia.Typeface)
        self.assertEqual(typeface.familyName(), face.familyName())
        self.assertEqual(weave.Face(typeface), face)
        self.assertEqual(skia.Typeface(face).familyName(), face.familyName())
        style = weave.Type(face=typeface)
        self.assertEqual(style.face, face)
        self.assertIsInstance(style.face, weave.Face)

    def test_the_theme_carries_its_two_faces(self):
        theme = kit.houseTheme()
        self.assertIsInstance(theme.type.mono, weave.Face)
        # A null sans is the font context's own default family.
        self.assertIsNone(theme.type.sans)
        line = theme.mono(11, "#000000")
        self.assertEqual(line.shaping.typeface, theme.type.mono)
        self.assertIsInstance(line.shaping.typeface, weave.Face)
        self.assertEqual(theme.sans(12, "#000000", 0.5).shaping.letterSpacing, 0.5)


if __name__ == "__main__":
    unittest.main()
