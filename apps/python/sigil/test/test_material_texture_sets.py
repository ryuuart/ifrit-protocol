"""A texture set is loaded from files and worn as C++ wears it.

A folder a material authoring tool exports is grouped into sets by the
role words in its file names, each set decoded into one repeating texture
per role, and the decoded maps handed to the surface program. The demo
assets carry one such folder, `metal_plate`, fetched by `mise run assets`.
"""

import unittest
from pathlib import Path

from sigil import material
from sigil.material import surface, texture
from sigil.material.texture import Role, TextureMaps, TextureSet, Volume

METAL_PLATE = (
    Path(__file__).resolve().parents[3]
    / "spell-circle-canvas/build/assets/textures/metal_plate"
)


class TextureSets(unittest.TestCase):
    def test_a_file_name_is_read_for_its_role_and_set(self):
        classified = texture.classify("metal_plate_nor_dx_2k.png")
        self.assertEqual(classified.role, Role.Normal)
        self.assertEqual(classified.set, "metal_plate")
        self.assertTrue(classified.directX)
        self.assertEqual(texture.roleForUsage("diffuse"), Role.BaseColor)
        self.assertEqual(texture.name(Role.BaseColor), "baseColor")

    @unittest.skipUnless(METAL_PLATE.is_dir(), "the demo assets are not fetched")
    def test_a_set_loaded_from_the_demo_assets_reads_its_maps_back(self):
        sets = texture.discover(METAL_PLATE)
        self.assertEqual([found.name for found in sets], ["metal_plate"])
        files = sets[0].files
        self.assertEqual(files[Role.BaseColor].name, "metal_plate_diff_1k.png")
        self.assertIn(Role.Packed, files)

        maps = texture.fromFiles(sets[0])
        self.assertEqual(maps.name, "metal_plate")
        self.assertFalse(maps.normalDirectX)
        self.assertEqual(set(maps.maps), set(files))
        base = maps.map(Role.BaseColor)
        self.assertIsNotNone(base)
        self.assertTrue(base.valid())
        self.assertEqual(tuple(base.size()), (1024, 1024))
        self.assertEqual(base.tileX(), material.Repeat.Repeat)
        self.assertIsNone(maps.map(Role.Emissive))

        program = surface.program(maps, base=surface.SurfaceParameters(metallic=1))
        self.assertNotEqual(program, surface.program())

    @unittest.skipUnless(METAL_PLATE.is_dir(), "the demo assets are not fetched")
    def test_a_decoder_of_the_authors_own_is_asked_for_every_file(self):
        asked = []

        def decode(path):
            asked.append(Path(path).name)
            return None

        metalPlate = texture.discover(METAL_PLATE)[0]
        maps = texture.fromFiles(metalPlate, decode=decode)
        self.assertEqual(sorted(asked), sorted(path.name for path in metalPlate.files.values()))
        self.assertEqual(maps.maps, {})

    def test_the_records_are_stated_as_keywords(self):
        stated = TextureSet(name="tiles", normalDirectX=True)
        self.assertEqual((stated.name, stated.normalDirectX, stated.files), ("tiles", True, {}))
        volume = Volume(ior=1.33, attenuationDistance=2, dispersion=0.1)
        self.assertEqual(volume, Volume(ior=1.33, attenuationDistance=2, dispersion=0.1))
        self.assertNotEqual(volume, Volume())
        maps = TextureMaps(name="water", thickness=0.5, volume=volume)
        self.assertEqual(maps.volume, volume)
        self.assertEqual(maps.thickness, 0.5)
        self.assertIsNone(TextureMaps().volume)
        with self.assertRaises(TypeError):
            TextureMaps(colour=1)


if __name__ == "__main__":
    unittest.main()
