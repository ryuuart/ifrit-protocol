#!/usr/bin/env python3
"""Demo assets, fetched from the network into the build tree.

The studies in src/common/compose/gallery and sketch/sketches are
reference-grounded, and a reference set in whatever face the host OS
happens to ship is only half-grounded. This fetches the real
open-licensed faces those studies want, so a study can name a typeface
the way it names a hex value.

Opt in — nothing here runs as part of a build:

    scripts/fetch_assets.py            (or: mise run assets)
    cmake --build build --config Release --target fetch_assets

The result lands in build/assets/, which the sketch host reads with
`--assets <dir>` and the libraries find through the SIGIL_ASSET_DIR
compile definition. No build tree is needed: the script only writes
files.

Rules for anything added to the manifest below:
 * an OPEN licence, and the licence file is fetched alongside the asset;
 * pinned to an immutable commit, never a branch, so the URL cannot
   change under the hash;
 * a sha256, so a changed byte is a hard failure and not a silent
   substitution;
 * no game, film or museum rips. The studies reproduce GEOMETRY and
   PALETTES, which are facts about a design; they do not ship its art.

The downloader is shared: scripts/build_docs.py fetches the Doxygen
theme through fetch() with a manifest of its own.
"""

import argparse
import hashlib
import ssl
import sys
import urllib.error
import urllib.request
from pathlib import Path
from typing import NamedTuple


class Asset(NamedTuple):
    """One file to fetch: where from, where to, and what it must hash to."""

    url: str
    dest: str  # relative to the output directory
    sha256: str


# google/fonts, pinned. Every entry is SIL Open Font License 1.1.
_GFONTS = (
    "https://raw.githubusercontent.com/google/fonts/"
    "684b69db51d59a3137ec0152fa3a3afc6f1b3814"
)

ASSETS = [
    # Archivo — grotesque with a width axis: the HUD and Y2K studies
    Asset(
        f"{_GFONTS}/ofl/archivo/Archivo%5Bwdth,wght%5D.ttf",
        "fonts/Archivo.ttf",
        "0e094a7d3c7c4c25cf1310c4b30014f1dae9332220b1c2c88f4fa996f0b05053",
    ),
    Asset(
        f"{_GFONTS}/ofl/archivo/OFL.txt",
        "fonts/Archivo.OFL.txt",
        "108b4e57c9c796d3d38d0428ca7ee39de47ad93187302718d9b2d8864b9b716b",
    ),
    # Inter — the Swiss workhorse: the Gerstner and Brockmann grids
    Asset(
        f"{_GFONTS}/ofl/inter/Inter%5Bopsz,wght%5D.ttf",
        "fonts/Inter.ttf",
        "29160a80ff49ddcab2c97711247e08b1fab27a484a329ce8b813d820dc559031",
    ),
    Asset(
        f"{_GFONTS}/ofl/inter/OFL.txt",
        "fonts/Inter.OFL.txt",
        "5b9321a4298cfeb6b34354164a1c3afc3db114569984c502b9b35d988fd58c57",
    ),
    # JetBrains Mono — the console and every readout
    Asset(
        f"{_GFONTS}/ofl/jetbrainsmono/JetBrainsMono%5Bwght%5D.ttf",
        "fonts/JetBrainsMono.ttf",
        "48715a42ec242c21e9f02692891e147d022299a52e48d5e413e1a942193ffeda",
    ),
    Asset(
        f"{_GFONTS}/ofl/jetbrainsmono/OFL.txt",
        "fonts/JetBrainsMono.OFL.txt",
        "b2fe5e8987594e9ffd1d2ca52a2f5d73eb8335243893c5d6254b5ad69269591d",
    ),
    # EB Garamond — the manuscript, the plate lettering, the inscriptions
    Asset(
        f"{_GFONTS}/ofl/ebgaramond/EBGaramond%5Bwght%5D.ttf",
        "fonts/EBGaramond.ttf",
        "ef9512f92f6d579e5dc75af59a5a4b1b8b47d2eda89e00b954d44520e5369027",
    ),
    Asset(
        f"{_GFONTS}/ofl/ebgaramond/OFL.txt",
        "fonts/EBGaramond.OFL.txt",
        "0985066662eb755ed3683ae5482a81a9195b49ce3f7e165cc2388b3dbece7dd7",
    ),
    # Bebas Neue — condensed display, the poster studies
    Asset(
        f"{_GFONTS}/ofl/bebasneue/BebasNeue-Regular.ttf",
        "fonts/BebasNeue.ttf",
        "08e4623805102d819f58601e46e345648846075e363b2ceb23313c2d1c83ec73",
    ),
    Asset(
        f"{_GFONTS}/ofl/bebasneue/OFL.txt",
        "fonts/BebasNeue.OFL.txt",
        "72082f6cb4d04be2ecf7cc7d9e1e7d73787f0af8a5a278a47cade70c16b78341",
    ),
    # The Ghostscript tiger — the classic vector torture test, exercised
    # by SigilImage's SVG decode backend.
    # AGPL-3.0 (per its Wikimedia Commons file page).
    Asset(
        "https://upload.wikimedia.org/wikipedia/commons/f/fd/Ghostscript_Tiger.svg",
        "svg/tiger.svg",
        "5211e169283f43ab8ad7ea7998d917d5fbb3c568ac85c1a0217e86792822684d",
    ),
    Asset(
        "https://www.gnu.org/licenses/agpl-3.0.txt",
        "svg/tiger.LICENSE.txt",
        "0d96a4ff68ad6d4b6f1f30f713b18d5184912ba8dd389f86aa7710db079abcb0",
    ),
    # Poly Haven studio HDRI (CC0) — the literal-materials environment:
    # SigilGeometry's Environment::fromEquirect and the world lighting work.
    Asset(
        "https://dl.polyhaven.org/file/ph-assets/HDRIs/hdr/1k/studio_small_09_1k.hdr",
        "hdri/studio_small_09_1k.hdr",
        "e7cfda5f4e98e623db12b8bfd0184e048488e4855d9c83e2751fb44a32e80c45",
    ),
    Asset(
        "https://creativecommons.org/publicdomain/zero/1.0/legalcode.txt",
        "hdri/CC0.LICENSE.txt",
        "a2010f343487d3f7618affe54f789f5487602331c0a8d03f49e9a7c547cf0499",
    ),
    # Khronos glTF sample asset "Avocado" (CC0, textures embedded in the
    # GLB) — SigilGeometry's model import (the scattered_model sketch
    # names this file among the ones it looks for under models/).
    Asset(
        "https://raw.githubusercontent.com/KhronosGroup/glTF-Sample-Assets/"
        "2bac6f8c57bf471df0d2a1e8a8ec023c7801dddf/Models/Avocado/"
        "glTF-Binary/Avocado.glb",
        "models/Avocado.glb",
        "ccc9c3ce56423720b09399c2351537207cd5a65f859f9e6e2f30922762f3abd4",
    ),
    Asset(
        "https://raw.githubusercontent.com/KhronosGroup/glTF-Sample-Assets/"
        "2bac6f8c57bf471df0d2a1e8a8ec023c7801dddf/Models/Avocado/LICENSE.md",
        "models/Avocado.LICENSE.md",
        "15aa885ef74db3dce103da85abb568476378a815ca46db0cf7667a1a795b4194",
    ),
]

# Poly Haven "metal_plate" PBR texture set (CC0), one file per map in the
# site's own naming (_diff, _nor_gl, _rough, _metal, _ao, and the packed
# _arm) — SigilMaterial's texture-set discovery recognizes these names.
_PLATE = "https://dl.polyhaven.org/file/ph-assets/Textures/png/1k/metal_plate"
ASSETS += [
    Asset(
        f"{_PLATE}/metal_plate_{map_name}_1k.png",
        f"textures/metal_plate/metal_plate_{map_name}_1k.png",
        digest,
    )
    for map_name, digest in (
        ("diff", "00401b4ce56f0ffa8b2c1e10366fa36a7a9b839199b3abac2678790a9883c2e9"),
        ("nor_gl", "d858021ebc0a2a01ec13e574bc47ed58795e129904cda2868b3fb2dfd02d6741"),
        ("rough", "aa6e4947c33559a7012e275d1600bfa6e00c2824271726a94de69f1a7b7fed2e"),
        ("metal", "394ee114df29630336322edbbd3ca9a7782b040deea58f620b0e298c973c6ace"),
        ("ao", "70653401fb0f992daf58c7b51cc4aef469bf0e4eed484c6ad1d57c93e50ae361"),
        ("arm", "d568d7ef7c206b3737b4fa38ba7984037f1ee2afbfd2ef03dd4418e8975eec04"),
    )
]
ASSETS.append(
    Asset(
        "https://creativecommons.org/publicdomain/zero/1.0/legalcode.txt",
        "textures/metal_plate/CC0.LICENSE.txt",
        "a2010f343487d3f7618affe54f789f5487602331c0a8d03f49e9a7c547cf0499",
    )
)

# Some hosts answer a request with no product name with a block page,
# which would arrive as a hash mismatch and read as a corrupt asset.
USER_AGENT = "spell-circle-canvas-fetch-assets"


def sha256_of(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as handle:
        for block in iter(lambda: handle.read(1 << 20), b""):
            digest.update(block)
    return digest.hexdigest()


def fetch(assets, out: Path, label: str = "assets") -> None:
    """Downloads what is missing or wrong, leaves what already matches.

    The hash is the contract: a file already at its destination is kept
    only when it hashes to what the manifest says, and a download whose
    bytes hash to something else is removed rather than left in place to
    be read later as the asset it is not.
    """
    context = ssl.create_default_context()
    for asset in assets:
        target = out / asset.dest
        if target.exists() and sha256_of(target) == asset.sha256:
            print(f"have {asset.dest}")
            continue
        print(f"fetch {asset.dest}")
        target.parent.mkdir(parents=True, exist_ok=True)
        request = urllib.request.Request(asset.url, headers={"User-Agent": USER_AGENT})
        try:
            with urllib.request.urlopen(request, context=context) as response:
                payload = response.read()
        except (urllib.error.URLError, OSError) as failure:
            sys.exit(f"{asset.dest}: {failure}")
        got = hashlib.sha256(payload).hexdigest()
        if got != asset.sha256:
            sys.exit(f"{asset.dest}: expected sha256 {asset.sha256}, got {got}")
        target.write_bytes(payload)
    print(f"{label} in {out}")


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument(
        "--out",
        type=Path,
        default=Path(__file__).resolve().parent.parent / "build" / "assets",
        help="where to write the assets (default: build/assets)",
    )
    args = parser.parse_args()
    fetch(ASSETS, args.out.resolve())


if __name__ == "__main__":
    main()
