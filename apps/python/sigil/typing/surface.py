"""The one table that says where a raw binding module surfaces publicly.

Every binding registers its module under the extension root `_sigil`.
Nothing an author writes names that root: the generator reads this table to
write the public declarations and the public re-export modules, and the
bindings read a mirror of it to tell each bound class which module it comes
from. Adding a raw module to the public surface is one row here; a raw module
with no row stops generation by name rather than appearing or disappearing
silently.

Two raw modules may share one public module. That is how a backend feature
reaches an author under the catalogue it serves instead of as a namespace of
its own. Names must still be unique across the merge; generation fails on a
collision.
"""

from __future__ import annotations

RAW_ROOT = "_sigil"
PUBLIC_ROOT = "sigil"

# Raw module -> the module an author imports it from. A value repeated across
# rows merges those raw modules into one public module.
PUBLIC_MODULES: dict[str, str] = {
    # The extension root carries the sketch context and file rendering, which
    # the sketch package owns. Its library submodules have rows of their own,
    # so they are not carried into the sketch package with it.
    "_sigil": "sigil.sketch",
    "_sigil._types": "sigil._types",
    "_sigil.compose": "sigil.compose",
    "_sigil.compose.brush": "sigil.compose.brush",
    "_sigil.compose.brush.presets": "sigil.compose.brush.presets",
    "_sigil.compose.brush.strand": "sigil.compose.brush.strand",
    "_sigil.compose.by": "sigil.compose.by",
    "_sigil.compose.decorations": "sigil.compose.decorations",
    "_sigil.compose.derive": "sigil.compose.derive",
    "_sigil.compose.document": "sigil.compose.document",
    "_sigil.compose.feed": "sigil.compose.feed",
    "_sigil.compose.fx": "sigil.compose.fx",
    "_sigil.compose.instancing": "sigil.compose.instancing",
    "_sigil.compose.instancing.place": "sigil.compose.instancing.place",
    "_sigil.compose.kit": "sigil.compose.kit",
    "_sigil.compose.kit.bevels": "sigil.compose.kit.bevels",
    "_sigil.compose.kit.flourish": "sigil.compose.kit.flourish",
    "_sigil.compose.kit.ornament": "sigil.compose.kit.ornament",
    "_sigil.compose.layouts": "sigil.compose.layouts",
    "_sigil.compose.lines": "sigil.compose.lines",
    "_sigil.compose.lines.presets": "sigil.compose.lines.presets",
    "_sigil.compose.parts": "sigil.compose.parts",
    "_sigil.compose.routers": "sigil.compose.routers",
    "_sigil.compose.selectors": "sigil.compose.selectors",
    "_sigil.compose.spans": "sigil.compose.spans",
    "_sigil.compose.styles": "sigil.compose.styles",
    "_sigil.compose.tiles": "sigil.compose.tiles",
    "_sigil.core": "sigil.core",
    "_sigil.core.chance": "sigil.core.chance",
    "_sigil.core.hardware": "sigil.core.hardware",
    "_sigil.core.hash": "sigil.core.hash",
    "_sigil.core.intervals": "sigil.core.intervals",
    "_sigil.core.noise": "sigil.core.noise",
    "_sigil.data": "sigil.data",
    "_sigil.draw": "sigil.draw",
    "_sigil.draw.brush": "sigil.draw.brush",
    "_sigil.draw.brush.format": "sigil.draw.brush.format",
    "_sigil.geometry": "sigil.geometry",
    "_sigil.geometry.arrange": "sigil.geometry.arrange",
    "_sigil.geometry.device": "sigil.geometry.device",
    "_sigil.geometry.mesh": "sigil.geometry.mesh",
    "_sigil.geometry.mesh.camera": "sigil.geometry.mesh.camera",
    "_sigil.geometry.mesh.codec": "sigil.geometry.mesh.codec",
    "_sigil.geometry.mesh.codec.decode": "sigil.geometry.mesh.codec.decode",
    "_sigil.geometry.mesh.codec.encode": "sigil.geometry.mesh.codec.encode",
    "_sigil.geometry.mesh.curve": "sigil.geometry.mesh.curve",
    "_sigil.geometry.mesh.kernel": "sigil.geometry.mesh.kernel",
    "_sigil.geometry.mesh.points": "sigil.geometry.mesh.points",
    "_sigil.geometry.mesh.pop": "sigil.geometry.mesh.pop",
    "_sigil.geometry.mesh.pop.profile": "sigil.geometry.mesh.pop.profile",
    "_sigil.geometry.mesh.render": "sigil.geometry.mesh.render",
    "_sigil.geometry.path": "sigil.geometry.path",
    "_sigil.geometry.path.blend": "sigil.geometry.path.blend",
    "_sigil.geometry.path.crossing": "sigil.geometry.path.crossing",
    "_sigil.geometry.path.operations": "sigil.geometry.path.operations",
    "_sigil.geometry.path.profile": "sigil.geometry.path.profile",
    "_sigil.geometry.sections": "sigil.geometry.sections",
    "_sigil.geometry.shapers": "sigil.geometry.shapers",
    "_sigil.geometry.shapes": "sigil.geometry.shapes",
    "_sigil.image": "sigil.image",
    "_sigil.io": "sigil.io",
    "_sigil.io.publish": "sigil.io.publish",
    "_sigil.material": "sigil.material",
    "_sigil.material.field": "sigil.material.field",
    "_sigil.material.kit": "sigil.material.kit",
    "_sigil.material.ocio": "sigil.material.ocio",
    "_sigil.material.pattern": "sigil.material.pattern",
    "_sigil.material.sdf": "sigil.material.sdf",
    # The Skia backend of SigilMaterial is one executor of the material
    # catalogue, not a catalogue of peers beside field, kit and pattern, and
    # it is the only executor Python reaches. Its paint, effect, fit and
    # bloom stand in the material module itself.
    "_sigil.material.skia": "sigil.material",
    "_sigil.material.slang": "sigil.material.slang",
    "_sigil.material.stock": "sigil.material.stock",
    "_sigil.material.texture": "sigil.material.texture",
    "_sigil.measure": "sigil.measure",
    "_sigil.motion": "sigil.motion",
    "_sigil.motion.ease": "sigil.motion.ease",
    "_sigil.motion.physics": "sigil.motion.physics",
    "_sigil.scry": "sigil.scry",
    "_sigil.sketch": "sigil.sketch",
    "_sigil.sketch.kit": "sigil.sketch.kit",
    "_sigil.sketch.scry": "sigil.sketch.scry",
    "_sigil.skia": "sigil.skia",
    "_sigil.skia.draw": "sigil.skia.draw",
    "_sigil.substance": "sigil.substance",
    "_sigil.usd": "sigil.usd",
    "_sigil.video": "sigil.video",
    "_sigil.weave": "sigil.weave",
    "_sigil.weave.features": "sigil.weave.features",
    "_sigil.weave.flowshape": "sigil.weave.flowshape",
    "_sigil.weave.kit": "sigil.weave.kit",
    "_sigil.weave.kit.hanging": "sigil.weave.kit.hanging",
    "_sigil.weave.kit.kinsoku": "sigil.weave.kit.kinsoku",
    "_sigil.weave.paint": "sigil.weave.paint",
    "_sigil.weave.ports": "sigil.weave.ports",
    "_sigil.weave.selectors": "sigil.weave.selectors",
    "_sigil.weave.unicode": "sigil.weave.unicode",
    "_sigil.world": "sigil.world",
    "_sigil.world.diligent": "sigil.world.diligent",
    "_sigil.world.graph": "sigil.world.graph",
    "_sigil.world.kit": "sigil.world.kit",
    "_sigil.world.light": "sigil.world.light",
    "_sigil.world.selectors": "sigil.world.selectors",
}

# Public module -> the raw spelling an author never types, and what they type
# instead. A wrapper in the module's addition may then shadow the renamed
# name with one that takes Python's keyword forms.
RENAMES: dict[str, dict[str, str]] = {
    "sigil.compose.document": {"listGap": "list_gap", "quoteInset": "quote_inset"},
    "sigil.compose.kit": {
        "captionLabel": "caption_label",
        "captionNote": "caption_note",
        "panelGrid": "panel_grid",
    },
    "sigil.compose.layouts": {"repeatTrack": "repeat_track"},
    "sigil.sketch": {"Context": "SketchContext"},
    "sigil.sketch.kit": {
        "houseFace": "house_face",
        "houseTheme": "house_theme",
        "panelGrid": "panel_grid",
        "studyTheme": "study_theme",
    },
}

# The raw modules a build may not carry, because the SDK behind the native
# library is a licensed download. A row here that this extension does not
# register is skipped rather than refused, and the package module already
# committed for it is left alone.
OPTIONAL_MODULES: frozenset[str] = frozenset(
    {"_sigil.scry", "_sigil.substance", "_sigil.usd", "_sigil.sketch.scry"}
)

# The named unions the bindings convert through. They are declarations and
# nothing else: no module of this name is registered or imported at runtime.
DECLARATION_ONLY: frozenset[str] = frozenset({"_sigil._types"})


def public_module(raw: str) -> str:
    """Return the public module a raw binding module surfaces in."""
    try:
        return PUBLIC_MODULES[raw]
    except KeyError:
        raise KeyError(
            f"{raw} has no public module. Add one row to PUBLIC_MODULES in "
            "typing/surface.py naming where an author imports it from."
        ) from None


def public_name(public: str, name: str) -> str:
    """Return the spelling an author writes for one member of a module."""
    return RENAMES.get(public, {}).get(name, name)


def parent_module(module: str) -> str:
    """Return the module that holds this one, or an empty string at the root."""
    return module.rpartition(".")[0]


def child_modules(public: str) -> list[str]:
    """Return the public modules whose parent is this one, in import order.

    A module with children is a directory with an __init__ of its own, and
    one without is a file. Every public module is one or the other, so an
    import statement finds the same module an attribute does and no module
    depends on its parent having been imported first.
    """
    return sorted(
        {
            candidate
            for candidate in PUBLIC_MODULES.values()
            if parent_module(candidate) == public
        }
    )


def raw_modules(public: str) -> list[str]:
    """Return the raw modules merged into one public module, in table order."""
    return [raw for raw, target in PUBLIC_MODULES.items() if target == public]
