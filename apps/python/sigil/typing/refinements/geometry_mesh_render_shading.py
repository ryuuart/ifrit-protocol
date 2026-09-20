"""The draw runtime seam, drawPanel and the environment.

Input contracts for the erased signatures of the
geometry-mesh/render-shading package, and nothing else: a fragment is one
author's alone.

GLM's caster reports only 'tuple', so every vector says how many numbers
it holds. A ROTATION IS COUNTED BY COLUMNS, as the camera's matrix is:
the three vectors are where the panorama's own x, y and z axes point.
"""

from __future__ import annotations

from .table import Table

MODULE = "_sigil.geometry.mesh.render"
ENVIRONMENT = MODULE + ".Environment"
LIGHT = MODULE + ".Light"
STYLE = MODULE + ".MeshStyle"
IMAGE = "_sigil.skia.Image"


def register(table: Table) -> None:
    """Record what pybind11 erased from this package's signatures."""
    # Every draw is handed the pen the frame is being drawn with; the
    # binding takes it as a handle and checks it, which leaves nothing in
    # the signature to say what it is.
    table.erased(
        MODULE, "drawBackdrop drawImagePanel drawMesh drawPanel", "_sigil.draw.Pen"
    )
    # A panel's body draws through a pen of its own, as every other
    # drawing callback does.
    table.parameters(MODULE + ".drawPanel", draw="_t.DrawCallback")
    table.parameters(LIGHT + ".__init__", direction="_t.Vec3Like")

    # A panorama that was never given is read as black rather than
    # refused, so every slot holding one reads back as absent too.
    table.attribute(ENVIRONMENT + ".irradiance", IMAGE + " | None")
    table.attribute(ENVIRONMENT + ".nextIrradiance", IMAGE + " | None")
    table.attribute(STYLE + ".texture", IMAGE + " | None")
    table.declares(
        ENVIRONMENT,
        "orientation",
        "@property\n"
        "def orientation(self) -> tuple[_t.Vec3, _t.Vec3, _t.Vec3]: ...\n"
        "@orientation.setter\n"
        "def orientation(self, value: collections.abc.Sequence[_t.Vec3Like])"
        " -> None: ...\n",
    )

    # THE SHADING TERMS. Each takes and answers a direction, a radiance or
    # a reflectance, and every one of those is three numbers; the two
    # panorama coordinates and the split-sum pair are two.
    table.returns(
        MODULE,
        "attenuate backdropRay environmentIrradiance environmentRadiance"
        " environmentSpecular fresnelRough refraction specularColor toneMap",
        "_t.Vec3",
    )
    table.returns(MODULE, "environmentBrdf equirectangularUv", "_t.Vec2")
    table.parameters(MODULE + ".backdropRay", eye="_t.Vec3Like", ray="_t.Vec3Like")
    table.parameters(MODULE + ".equirectangularUv", direction="_t.Vec3Like")
    table.parameters(MODULE + ".specularColor", baseColor="_t.Vec3Like")
    table.parameters(MODULE + ".fresnelRough", f0="_t.Vec3Like")
    table.parameters(
        MODULE + ".environmentSpecular", radiance="_t.Vec3Like", f0="_t.Vec3Like"
    )
    table.parameters(
        MODULE + ".attenuate", radiance="_t.Vec3Like", absorb="_t.Vec3Like"
    )
    table.parameters(MODULE + ".luminance", color="_t.Vec3Like")
    table.parameters(MODULE + ".toneMap", radiance="_t.Vec3Like")
    table.parameters(
        MODULE + ".refraction", incident="_t.Vec3Like", normal="_t.Vec3Like"
    )
    # The whole declaration, because a panorama nobody gave is read as
    # black rather than refused and the generated signature has no way to
    # say so on a parameter it already spells.
    table.declares(
        MODULE,
        "samplePanorama",
        "def samplePanorama(panorama: " + IMAGE + " | None,"
        " uv: _t.Vec2Like) -> _t.Vec3: ...\n",
    )
    table.parameters(MODULE + ".environmentRadiance", direction="_t.Vec3Like")
    table.parameters(MODULE + ".environmentIrradiance", normal="_t.Vec3Like")
