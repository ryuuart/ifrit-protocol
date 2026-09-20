"""The Skia, image and core values, each registered from its own file."""

import copy
import gc
import unittest

from sigil import image, skia
from sigil.core import chance

# Two by two, opaque, one channel high in each texel: the pixels survive a
# byte-for-byte comparison through an unpremultiplied read-back and through
# a lossless encoding.
PIXELS = bytes(
    [255, 0, 0, 255, 0, 255, 0, 255, 0, 0, 255, 255, 255, 255, 255, 255]
)


class SkiaValues(unittest.TestCase):
    def test_every_moved_skia_name_is_still_there(self):
        for name in (
            "BlendMode",
            "FilterMode",
            "Image",
            "Matrix",
            "MipmapMode",
            "Paint",
            "PaintStyle",
            "Path",
            "PathBuilder",
            "PathDirection",
            "PathFillType",
            "PathOp",
            "Picture",
            "Point",
            "Rect",
            "RuntimeEffect",
            "SamplingOptions",
            "Size",
            "StrokeCap",
            "StrokeJoin",
            "TileMode",
            "Typeface",
            "VertexMode",
            "Vertices",
            "pathOp",
        ):
            self.assertTrue(hasattr(skia, name), name)

    def test_a_point_is_written_as_numbers_or_as_a_sequence(self):
        self.assertEqual(skia.Point(coordinates=(3, 4)).length(), 5)
        moved = skia.Point(x=1, y=2) + skia.Point(3, 4)
        self.assertEqual((moved.x, moved.y), (4, 6))
        halved = skia.Point(3, 4) / 2
        self.assertEqual((halved.x, halved.y), (1.5, 2))
        with self.assertRaises(ValueError):
            skia.Point(3, 4) / 0

    def test_a_rectangle_takes_its_corners_by_keyword(self):
        area = skia.Rect(x=10, y=20, width=30, height=40)
        self.assertEqual((area.width(), area.height()), (30, 40))
        self.assertEqual((area.left(), area.top()), (10, 20))
        self.assertTrue(area.contains(x=11, y=21))
        self.assertFalse(area.contains(x=9, y=21))
        self.assertEqual(skia.Rect((10, 20, 30, 40)).right(), area.right())
        self.assertEqual(skia.Size(width=2, height=3).width(), 2)

    def test_a_path_operation_answers_the_ring_it_cut(self):
        ring = skia.pathOp(
            skia.Path.Rect((0, 0, 100, 100)),
            skia.Path.Circle(x=50, y=50, radius=20),
            skia.PathOp.Difference,
        )
        self.assertTrue(ring.contains(5, 5))
        self.assertFalse(ring.contains(50, 50))
        self.assertEqual(ring.getBounds().width(), 100)

    def test_a_builder_hands_back_the_path_it_built(self):
        built = (
            skia.PathBuilder()
            .moveTo(x=0, y=0)
            .lineTo(x=10, y=0)
            .lineTo(x=10, y=10)
            .close()
            .detach()
        )
        self.assertFalse(built.isEmpty())
        self.assertEqual(
            (built.getBounds().width(), built.getBounds().height()), (10, 10)
        )
        moved = built.transform(skia.Matrix.Translate(x=5, y=0))
        self.assertEqual(moved.getBounds().left(), 5)


class ImageValues(unittest.TestCase):
    def test_every_moved_image_name_is_still_there(self):
        for name in (
            "Format",
            "ImageAsset",
            "decode",
            "decodeAsset",
            "encode",
            "from_rgba",
        ):
            self.assertTrue(hasattr(image, name), name)

    def test_pixels_are_copied_out_of_the_buffer_they_came_from(self):
        pixels = bytearray(PIXELS)
        made = image.from_rgba(pixels, width=2, height=2)
        pixels[:] = bytes(len(PIXELS))
        del pixels
        gc.collect()
        self.assertEqual((made.width(), made.height()), (2, 2))
        self.assertEqual(made.rgba(), PIXELS)

    def test_an_encoded_image_decodes_back_to_the_pixels_it_held(self):
        encoded = image.encode(
            image.from_rgba(PIXELS, 2, 2), format=image.Format.Png, quality=100
        )
        self.assertEqual(image.decode(data=encoded).rgba(), PIXELS)
        asset = image.decodeAsset(data=encoded, width=0, height=0, hint="")
        self.assertEqual((asset.width(), asset.height()), (2, 2))
        self.assertFalse(asset.animated())

    def test_a_frame_outlives_the_asset_that_decoded_it(self):
        asset = image.decodeAsset(image.encode(image.from_rgba(PIXELS, 2, 2)))
        frame = asset.frameAt(milliseconds=0)
        del asset
        gc.collect()
        self.assertEqual(frame.rgba(), PIXELS)

    def test_the_image_doors_answer_the_class_the_skia_file_registers(self):
        # The two libraries are bound from two files now, so an image door
        # naming a Skia value is the seam between them.
        made = image.from_rgba(PIXELS, 2, 2)
        self.assertIsInstance(made, skia.Image)
        self.assertIsInstance(image.decode(image.encode(made)), skia.Image)

    def test_a_refused_buffer_says_what_it_needed(self):
        with self.assertRaises(ValueError):
            image.from_rgba(bytes(3), 1, 1)
        with self.assertRaises(ValueError):
            image.from_rgba(bytes(4), 0, 1)
        with self.assertRaises(ValueError):
            image.decode(b"not an image")


class CoreValues(unittest.TestCase):
    def test_every_moved_chance_name_is_still_there(self):
        for name in ("Source", "Stream"):
            self.assertTrue(hasattr(chance, name), name)
        for name in (
            "Pcg",
            "Mix64",
            "Xorshift",
            "Halton",
            "Sobol",
            "Golden",
            "Stratified",
        ):
            self.assertTrue(hasattr(chance.Source, name), name)

    def test_a_copied_stream_continues_the_same_run(self):
        stream = chance.Stream.pcg(seed=7)
        opening = [stream.unit() for _ in range(3)]
        seeded = chance.Stream.pcg(7)
        self.assertEqual(opening, [seeded.unit() for _ in range(3)])
        continued = stream.copy()
        self.assertEqual(
            [stream.unit() for _ in range(3)],
            [continued.unit() for _ in range(3)],
        )
        shadow = copy.copy(continued)
        self.assertEqual(
            [stream.unit() for _ in range(2)], [shadow.unit() for _ in range(2)]
        )
        self.assertEqual(stream.source(), chance.Source.Pcg)
        self.assertEqual(stream.drawn(), 8)

    def test_the_one_factory_carries_the_source_and_its_parameter(self):
        named = chance.Stream.of(source=chance.Source.Halton, seed=0, parameter=3)
        rolled = chance.Stream.halton(base=3, skip=0)
        self.assertEqual(named.source(), chance.Source.Halton)
        self.assertEqual(named.parameter(), 3)
        self.assertEqual(
            [named.unit() for _ in range(4)], [rolled.unit() for _ in range(4)]
        )

    def test_every_draw_stays_inside_the_bounds_it_was_given(self):
        stream = chance.Stream.mix64(seed=11)
        self.assertTrue(all(0 <= stream.below(upper=4) < 4 for _ in range(32)))
        self.assertTrue(all(5 <= stream.range(low=5, high=6) < 6 for _ in range(32)))
        self.assertTrue(0 <= chance.Stream().unit() < 1)
        self.assertTrue(-1 <= chance.Stream.xorshift(3).signedUnit() < 1)
        self.assertGreaterEqual(chance.Stream.sobol(skip=2).bits(), 0)
        self.assertIsInstance(chance.Stream.golden(seed=1).normal(), float)
        self.assertTrue(0 <= chance.Stream.stratified(strata=4, seed=1).unit() < 1)


class PublicNaming(unittest.TestCase):
    def test_each_moved_value_names_the_module_it_is_imported_from(self):
        self.assertEqual(skia.Point.__module__, "sigil.skia")
        self.assertEqual(skia.Image.__module__, "sigil.skia")
        self.assertEqual(image.Format.__module__, "sigil.image")
        self.assertEqual(image.ImageAsset.__module__, "sigil.image")
        self.assertEqual(chance.Source.__module__, "sigil.core.chance")
        self.assertEqual(chance.Stream.__module__, "sigil.core.chance")


if __name__ == "__main__":
    unittest.main()
