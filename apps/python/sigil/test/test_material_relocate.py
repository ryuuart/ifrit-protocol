"""The material bindings after they were split into one file per subject.

Splitting the registrations moves no name: every spelling an author had
before is the spelling they have now, on the same module, reading the
same inputs. These cases are that promise, said out loud.
"""

import copy
import unittest

import _sigil
from sigil import material, skia

# The submodules of the material library, each registered from its own
# file and each re-exported by the package.
SUBMODULES = (
    "field",
    "kit",
    "ocio",
    "pattern",
    "sdf",
    "skia",
    "slang",
    "stock",
    "texture",
)

# What the four subjects that were written in another library's file
# registered before the split, module name by module name. A subject
# grows names afterwards, so a case asks that each of these is there
# and never that nothing else is.
MOVED_NAMES = {
    "field": (
        "CrtOverlayParameters",
        "crtOverlay",
        "grain",
        "halftoneRamp",
        "noise",
        "ripple",
    ),
    "kit": ("Reflection", "SurfaceParameters", "surface", "unlit"),
    "pattern": ("Tile", "checker", "gridLines", "halftone", "stripes"),
    "skia": ("BloomParameters", "Effect", "Fit", "Paint", "bloom"),
}

# The members the classes carried across with them: the fields a record
# is written by keyword, and the verbs and factories a value answers.
MOVED_MEMBERS = {
    ("field", "CrtOverlayParameters"): (
        "uBeamFalloff",
        "uBeamPitch",
        "uBeamStrength",
        "uBeatFalloff",
        "uBeatPitch",
        "uBeatStrength",
        "uGrain",
        "uScanPitch",
        "uScanStrength",
        "uSqueeze",
        "uVigInner",
        "uVigOuter",
        "uVigStrength",
    ),
    ("kit", "SurfaceParameters"): (
        "absorption",
        "alphaCutoff",
        "baseColor",
        "chrome",
        "dielectric",
        "emissive",
        "emissiveStrength",
        "glass",
        "gold",
        "ior",
        "metal",
        "metallic",
        "metallicChannel",
        "normalDirectX",
        "normalScale",
        "occlusionChannel",
        "occlusionStrength",
        "opacityChannel",
        "reflectionWeight",
        "roughness",
        "roughnessChannel",
        "thickness",
        "transmission",
    ),
    ("pattern", "Tile"): ("image", "offset", "paint", "rotate", "scale", "seed"),
    ("skia", "BloomParameters"): (
        "deepening",
        "dilation",
        "knee",
        "maxOpacity",
        "sigma",
        "softness",
        "spread",
        "strength",
        "tail",
        "threshold",
        "whitening",
    ),
    ("skia", "Effect"): (
        "brightPass",
        "blur",
        "deepen",
        "dilate",
        "directionalBlur",
        "emit",
        "glow",
        "isAnimated",
        "phosphorBloom",
        "recipe",
        "shader",
        "slot",
        "then",
        "uniform",
        "usesWorldSpace",
        "whiten",
    ),
    ("skia", "Paint"): (
        "amount",
        "blend",
        "conical",
        "copy",
        "fit",
        "glowUnit",
        "image",
        "isAnimated",
        "isNone",
        "linear",
        "linearUnit",
        "quantizeTime",
        "radial",
        "radialUnit",
        "recipe",
        "sksl",
        "slot",
        "solid",
        "sweep",
        "uniform",
        "worldSpace",
    ),
}

# The enumerations that moved, and the values an author writes.
MOVED_VALUES = {
    ("kit", "Reflection"): ("Additive", "SplitSum"),
    ("skia", "Fit"): ("Contain", "Cover", "Native", "Stretch"),
}


class MovedNames(unittest.TestCase):
    def test_every_subject_still_answers_at_the_path_it_had(self):
        self.assertTrue(hasattr(_sigil.material, "Color"))
        self.assertTrue(hasattr(_sigil.material.skia, "Paint"))
        self.assertTrue(hasattr(_sigil.material.pattern, "checker"))
        self.assertTrue(hasattr(_sigil.material.field, "noise"))
        self.assertTrue(hasattr(_sigil.material.kit, "SurfaceParameters"))

    def test_the_colour_leaf_carries_every_name_it_registered(self):
        for name in (
            "Backface",
            "Color",
            "Dither",
            "DitherKind",
            "HueArc",
            "Lab",
            "LinearRgb",
            "Oklab",
            "Oklch",
            "Palette",
            "PaletteMethod",
            "PaletteOptions",
            "Ramp",
            "RampBracket",
            "RampSpace",
            "RampStop",
            "Scheme",
            "closestEntry",
            "deltaE",
            "fitToSrgb",
            "fromLab",
            "fromOklab",
            "fromOklch",
            "harmony",
            "hsv",
            "inSrgbGamut",
            "lerpOklab",
            "lighten",
            "linearOf",
            "linearToSrgb",
            "luminance",
            "mixLinear",
            "mixToward",
            "oklabOf",
            "oklchOf",
            "palette",
            "ramp",
            "rampBracket",
            "rgb",
            "rotateHue",
            "sampleRamp",
            "scale",
            "srgbToLinear",
            "toLab",
            "toOklab",
            "toOklch",
            "withAlpha",
        ):
            self.assertTrue(hasattr(_sigil.material, name), name)

    def test_the_extension_module_lists_every_submodule(self):
        listed = dir(_sigil.material)
        for name in SUBMODULES:
            self.assertIn(name, listed, name)

    def test_the_package_re_exports_what_the_extension_registered(self):
        for name in SUBMODULES:
            if name == "skia":
                # The paint submodule is published as the package's own
                # material module, so a paint is one import from a colour.
                continue
            published = getattr(material, name)
            for member in dir(getattr(_sigil.material, name)):
                if member.startswith("_"):
                    continue
                self.assertTrue(hasattr(published, member), f"{name}.{member}")
        for name in ("BloomParameters", "Effect", "Fit", "Paint", "bloom"):
            self.assertIs(
                getattr(material, name), getattr(_sigil.material.skia, name), name
            )


class MovedInventories(unittest.TestCase):
    """The four subjects that were registered in another library's file.

    The colour leaf never left its file, so the list above is enough for
    it. These four were written in the Skia file and the world file and
    now stand in the material files named for them, which is the move a
    dropped name would hide in.
    """

    def test_every_moved_module_still_answers_each_name(self):
        for module, names in MOVED_NAMES.items():
            registered = getattr(_sigil.material, module)
            for name in names:
                with self.subTest(module=module, name=name):
                    self.assertTrue(hasattr(registered, name))

    def test_every_moved_class_still_carries_each_member(self):
        for (module, class_name), members in MOVED_MEMBERS.items():
            registered = getattr(getattr(_sigil.material, module), class_name)
            for member in members:
                with self.subTest(owner=f"{module}.{class_name}", member=member):
                    self.assertTrue(hasattr(registered, member))

    def test_every_moved_enumeration_still_names_each_value(self):
        for (module, class_name), values in MOVED_VALUES.items():
            registered = getattr(getattr(_sigil.material, module), class_name)
            for value in values:
                with self.subTest(owner=f"{module}.{class_name}", value=value):
                    self.assertTrue(hasattr(registered, value))

    def test_a_material_is_still_one_kind_of_paint(self):
        # The conversion that lets a recipe stand where a paint is asked
        # for is registered beside the paint, and the recipe class it
        # names is registered by a file that runs ahead of it: the order
        # the subjects are registered in is what keeps a recipe a value
        # these two take rather than a type error.
        recipe = material.field.noise(0.02)
        self.assertIsInstance(material.Effect.blur(recipe, 4.0), material.Effect)
        self.assertIsInstance(
            material.Paint.blend([(recipe, skia.BlendMode.Multiply)]), material.Paint
        )

    def test_a_surface_colour_goes_back_out_the_way_it_came_in(self):
        # The surface's colour fields are factors on the maps in their
        # slots, and the property the move brought across converts
        # nothing either way, so the parameter answers the colour class
        # it was written with, at the value it was written with.
        parameters = material.kit.SurfaceParameters(baseColor="#e75a31")
        read = parameters.baseColor
        self.assertIsInstance(read, material.Color)
        for written, answered in zip(material.Color("#e75a31"), read):
            self.assertAlmostEqual(written, answered, places=3)
        self.assertIsInstance(material.kit.surface(parameters), material.Material)
        self.assertIsInstance(
            material.kit.surface(
                parameters=parameters, reflection=material.kit.Reflection.Additive
            ),
            material.Material,
        )


class TheFieldSubject(unittest.TestCase):
    """The per-pixel surfaces, registered from the field file."""

    def test_every_field_verb_answers_a_material(self):
        field = material.field
        for made in (
            field.ripple(4, 40),
            field.ripple(amplitudePx=4, wavelengthPx=40, phase=0.5, vertical=True),
            field.halftoneRamp(6, 1, 2, "#ff0000"),
            field.crtOverlay(),
            field.noise(0.02),
            field.grain(0.5),
        ):
            self.assertIsInstance(made, material.Material)

    def test_the_halftone_ramp_reads_every_colour_spelling(self):
        for value in ("#ff0000", (1, 0, 0), [1, 0, 0, 1], material.Color(1, 0, 0)):
            with self.subTest(colour=value):
                made = material.field.halftoneRamp(
                    spacing=6, rMin=1, rMax=2, color=value
                )
                self.assertIsInstance(made, material.Material)

    def test_the_tube_takes_its_parameters_by_keyword(self):
        parameters = material.field.CrtOverlayParameters(uScanPitch=6.0)
        self.assertEqual(parameters.uScanPitch, 6.0)
        self.assertEqual(
            parameters.uBeamPitch,
            material.field.CrtOverlayParameters().uBeamPitch,
        )
        parameters.uGrain = 0.25
        self.assertEqual(copy.copy(parameters).uGrain, 0.25)
        self.assertEqual(copy.deepcopy(parameters).uGrain, 0.25)
        self.assertIsInstance(
            material.field.crtOverlay(parameters=parameters), material.Material
        )

    def test_a_parameter_the_tube_does_not_have_says_so(self):
        with self.assertRaisesRegex(TypeError, "CRT"):
            material.field.CrtOverlayParameters(scanPitch=6.0)


class TheColourSubject(unittest.TestCase):
    """The colour leaf, which stayed in the file the split began with."""

    def test_a_colour_is_written_as_channels_or_as_a_string(self):
        self.assertEqual(material.Color("#ff0000"), material.Color(1, 0, 0, 1))
        self.assertEqual(material.rgb(0xFF0000), material.Color(1, 0, 0, 1))
        self.assertEqual(tuple(material.Color(red=1, green=0, blue=0)), (1, 0, 0, 1))

    def test_a_ramp_answers_the_colour_it_was_built_from(self):
        ramp = material.Ramp(
            stops=[
                material.RampStop(0.0, "#000000"),
                material.RampStop(1.0, "#ffffff"),
            ]
        )
        self.assertEqual(ramp.at(0.0), material.Color(0, 0, 0, 1))
        self.assertEqual(ramp(1.0), material.Color(1, 1, 1, 1))
        self.assertEqual(ramp, ramp.copy())

    def test_a_palette_is_indexed_and_iterated(self):
        palette = material.Palette(["#000000", "#ffffff"])
        self.assertEqual(len(palette), 2)
        self.assertEqual(palette[-1], material.Color(1, 1, 1, 1))
        self.assertEqual(list(palette)[0], material.Color(0, 0, 0, 1))


class PublicNaming(unittest.TestCase):
    def test_each_moved_value_names_the_module_it_is_imported_from(self):
        self.assertEqual(material.Color.__module__, "sigil.material")
        self.assertEqual(material.Material.__module__, "sigil.material")
        self.assertEqual(material.Paint.__module__, "sigil.material")
        self.assertEqual(
            material.field.CrtOverlayParameters.__module__, "sigil.material.field"
        )
        self.assertEqual(material.pattern.Tile.__module__, "sigil.material.pattern")
        self.assertEqual(
            material.kit.SurfaceParameters.__module__, "sigil.material.kit"
        )


class RegistrationOrder(unittest.TestCase):
    """A type has to be registered before a signature names it.

    A file registered later than the one whose signatures mention its
    types leaves those signatures spelled in C++, which is what an
    author reads in `help()` and what the declaration generator copies.
    Splitting the subjects moved each registration, so these are the
    signatures that cross from one file to another.
    """

    def test_a_crossing_signature_carries_the_python_spelling(self):
        for named, spelled in (
            (_sigil.material.pattern.Tile.paint, "_sigil.material.skia.Paint"),
            (_sigil.material.skia.Effect.recipe, "_sigil.material.Material"),
            (_sigil.material.skia.Paint.recipe, "_sigil.material.Material"),
            (_sigil.material.kit.surface, "_sigil.material.kit.SurfaceParameters"),
            (_sigil.material.kit.unlit, "_sigil.material.Material"),
            (
                _sigil.material.field.crtOverlay,
                "_sigil.material.field.CrtOverlayParameters",
            ),
            (_sigil.material.field.halftoneRamp, "_sigil.material.Material"),
        ):
            with self.subTest(named=named.__name__):
                self.assertIn(spelled, named.__doc__)
                self.assertNotIn("sigil::", named.__doc__)


if __name__ == "__main__":
    unittest.main()
