# Demo assets, fetched from the network into the build tree.
#
# The studies in src/common/compose/gallery and sketch/sketches are
# reference-grounded, and a reference set in whatever face the host OS
# happens to ship is only half-grounded. This target downloads the real
# open-licensed faces those studies want, so a study can name a typeface
# the way it names a hex value.
#
# Opt in — nothing here runs as part of `all`:
#
#     cmake --build build --config Release --target fetch_assets
#
# and the result lands in ${CMAKE_BINARY_DIR}/assets/, which the sketch
# host reads with `--assets <dir>` and the gallery finds through the
# SIGILCOMPOSE_ASSET_DIR compile definition.
#
# Rules for anything added to the manifest below:
#  * an OPEN licence, and the licence file is fetched alongside the asset;
#  * pinned to an immutable commit, never a branch, so the URL cannot
#    change under the hash;
#  * an EXPECTED_HASH, so a changed byte is a hard failure and not a
#    silent substitution;
#  * no game, film or museum rips. The studies reproduce GEOMETRY and
#    PALETTES, which are facts about a design; they do not ship its art.

set(SIGIL_ASSET_DIR "${CMAKE_BINARY_DIR}/assets" CACHE PATH
    "Where fetch_assets writes downloaded demo assets")

# google/fonts, pinned. Every entry is SIL Open Font License 1.1.
set(_gfonts_commit 684b69db51d59a3137ec0152fa3a3afc6f1b3814)
set(_gfonts_base
    "https://raw.githubusercontent.com/google/fonts/${_gfonts_commit}")

# path-under-google/fonts | destination | sha256
set(SIGIL_ASSET_MANIFEST
    # Archivo — grotesque with a width axis: the HUD and Y2K studies
    "ofl/archivo/Archivo%5Bwdth,wght%5D.ttf|fonts/Archivo.ttf|0e094a7d3c7c4c25cf1310c4b30014f1dae9332220b1c2c88f4fa996f0b05053"
    "ofl/archivo/OFL.txt|fonts/Archivo.OFL.txt|108b4e57c9c796d3d38d0428ca7ee39de47ad93187302718d9b2d8864b9b716b"
    # Inter — the Swiss workhorse: the Gerstner and Brockmann grids
    "ofl/inter/Inter%5Bopsz,wght%5D.ttf|fonts/Inter.ttf|29160a80ff49ddcab2c97711247e08b1fab27a484a329ce8b813d820dc559031"
    "ofl/inter/OFL.txt|fonts/Inter.OFL.txt|5b9321a4298cfeb6b34354164a1c3afc3db114569984c502b9b35d988fd58c57"
    # JetBrains Mono — the console and every readout
    "ofl/jetbrainsmono/JetBrainsMono%5Bwght%5D.ttf|fonts/JetBrainsMono.ttf|48715a42ec242c21e9f02692891e147d022299a52e48d5e413e1a942193ffeda"
    "ofl/jetbrainsmono/OFL.txt|fonts/JetBrainsMono.OFL.txt|b2fe5e8987594e9ffd1d2ca52a2f5d73eb8335243893c5d6254b5ad69269591d"
    # EB Garamond — the manuscript, the plate lettering, the inscriptions
    "ofl/ebgaramond/EBGaramond%5Bwght%5D.ttf|fonts/EBGaramond.ttf|ef9512f92f6d579e5dc75af59a5a4b1b8b47d2eda89e00b954d44520e5369027"
    "ofl/ebgaramond/OFL.txt|fonts/EBGaramond.OFL.txt|0985066662eb755ed3683ae5482a81a9195b49ce3f7e165cc2388b3dbece7dd7"
    # Bebas Neue — condensed display, the poster studies
    "ofl/bebasneue/BebasNeue-Regular.ttf|fonts/BebasNeue.ttf|08e4623805102d819f58601e46e345648846075e363b2ceb23313c2d1c83ec73"
    "ofl/bebasneue/OFL.txt|fonts/BebasNeue.OFL.txt|72082f6cb4d04be2ecf7cc7d9e1e7d73787f0af8a5a278a47cade70c16b78341"
)

# Non-font assets, each with a FULL url (different hosts). Same rules.
# full-url | destination | sha256
set(SIGIL_ASSET_URL_MANIFEST
    # The Ghostscript tiger — the classic vector torture test, exercised
    # by SigilImage's SVG decode backend and the world_demo SVG panel.
    # AGPL-3.0 (per its Wikimedia Commons file page).
    "https://upload.wikimedia.org/wikipedia/commons/f/fd/Ghostscript_Tiger.svg|svg/tiger.svg|5211e169283f43ab8ad7ea7998d917d5fbb3c568ac85c1a0217e86792822684d"
    "https://www.gnu.org/licenses/agpl-3.0.txt|svg/tiger.LICENSE.txt|0d96a4ff68ad6d4b6f1f30f713b18d5184912ba8dd389f86aa7710db079abcb0"
    # Poly Haven studio HDRI (CC0) — the literal-materials environment:
    # SigilShape's Environment::fromEquirect and the world lighting work.
    "https://dl.polyhaven.org/file/ph-assets/HDRIs/hdr/1k/studio_small_09_1k.hdr|hdri/studio_small_09_1k.hdr|e7cfda5f4e98e623db12b8bfd0184e048488e4855d9c83e2751fb44a32e80c45"
    "https://creativecommons.org/publicdomain/zero/1.0/legalcode.txt|hdri/CC0.LICENSE.txt|a2010f343487d3f7618affe54f789f5487602331c0a8d03f49e9a7c547cf0499"
    # Khronos glTF sample asset "Avocado" (CC0, textures embedded in the
    # GLB) — SigilShape's model import (shape_demo's imported-models
    # panel picks up anything under models/).
    "https://raw.githubusercontent.com/KhronosGroup/glTF-Sample-Assets/2bac6f8c57bf471df0d2a1e8a8ec023c7801dddf/Models/Avocado/glTF-Binary/Avocado.glb|models/Avocado.glb|ccc9c3ce56423720b09399c2351537207cd5a65f859f9e6e2f30922762f3abd4"
    "https://raw.githubusercontent.com/KhronosGroup/glTF-Sample-Assets/2bac6f8c57bf471df0d2a1e8a8ec023c7801dddf/Models/Avocado/LICENSE.md|models/Avocado.LICENSE.md|15aa885ef74db3dce103da85abb568476378a815ca46db0cf7667a1a795b4194"
    # Poly Haven "metal_plate" PBR texture set (CC0), one file per map in
    # the site's own naming (_diff, _nor_gl, _rough, _metal, _ao, and the
    # packed _arm) — SigilWorld's texture-set loader recognizes these
    # names, and world_demo dresses its material lab with them.
    "https://dl.polyhaven.org/file/ph-assets/Textures/png/1k/metal_plate/metal_plate_diff_1k.png|textures/metal_plate/metal_plate_diff_1k.png|00401b4ce56f0ffa8b2c1e10366fa36a7a9b839199b3abac2678790a9883c2e9"
    "https://dl.polyhaven.org/file/ph-assets/Textures/png/1k/metal_plate/metal_plate_nor_gl_1k.png|textures/metal_plate/metal_plate_nor_gl_1k.png|d858021ebc0a2a01ec13e574bc47ed58795e129904cda2868b3fb2dfd02d6741"
    "https://dl.polyhaven.org/file/ph-assets/Textures/png/1k/metal_plate/metal_plate_rough_1k.png|textures/metal_plate/metal_plate_rough_1k.png|aa6e4947c33559a7012e275d1600bfa6e00c2824271726a94de69f1a7b7fed2e"
    "https://dl.polyhaven.org/file/ph-assets/Textures/png/1k/metal_plate/metal_plate_metal_1k.png|textures/metal_plate/metal_plate_metal_1k.png|394ee114df29630336322edbbd3ca9a7782b040deea58f620b0e298c973c6ace"
    "https://dl.polyhaven.org/file/ph-assets/Textures/png/1k/metal_plate/metal_plate_ao_1k.png|textures/metal_plate/metal_plate_ao_1k.png|70653401fb0f992daf58c7b51cc4aef469bf0e4eed484c6ad1d57c93e50ae361"
    "https://dl.polyhaven.org/file/ph-assets/Textures/png/1k/metal_plate/metal_plate_arm_1k.png|textures/metal_plate/metal_plate_arm_1k.png|d568d7ef7c206b3737b4fa38ba7984037f1ee2afbfd2ef03dd4418e8975eec04"
    "https://creativecommons.org/publicdomain/zero/1.0/legalcode.txt|textures/metal_plate/CC0.LICENSE.txt|a2010f343487d3f7618affe54f789f5487602331c0a8d03f49e9a7c547cf0499"
)

# The fetch itself runs at BUILD time in script mode, so configuring the
# project never touches the network.
set(_fetch_script "${CMAKE_CURRENT_BINARY_DIR}/fetch_assets.cmake")
file(WRITE "${_fetch_script}" "# generated by cmake/FetchAssets.cmake\n")
file(APPEND "${_fetch_script}" "set(base \"${_gfonts_base}\")\n")
file(APPEND "${_fetch_script}" "set(out \"${SIGIL_ASSET_DIR}\")\n")
# Emits the have-check + hash-pinned download for one entry; `url` is
# the RESOLVED download url.
function(_sigil_append_fetch url dst hash)
  file(APPEND "${_fetch_script}" "
if(EXISTS \"\${out}/${dst}\")
  file(SHA256 \"\${out}/${dst}\" have)
  if(have STREQUAL \"${hash}\")
    message(STATUS \"have ${dst}\")
    set(skip TRUE)
  else()
    set(skip FALSE)
  endif()
else()
  set(skip FALSE)
endif()
if(NOT skip)
  message(STATUS \"fetch ${dst}\")
  file(DOWNLOAD \"${url}\" \"\${out}/${dst}\"
       EXPECTED_HASH SHA256=${hash}
       TLS_VERIFY ON
       STATUS status)
  list(GET status 0 code)
  if(NOT code EQUAL 0)
    list(GET status 1 why)
    file(REMOVE \"\${out}/${dst}\")
    message(FATAL_ERROR \"${dst}: \${why}\")
  endif()
endif()
")
endfunction()

foreach(entry IN LISTS SIGIL_ASSET_MANIFEST)
  string(REPLACE "|" ";" parts "${entry}")
  list(GET parts 0 src)
  list(GET parts 1 dst)
  list(GET parts 2 hash)
  _sigil_append_fetch("\${base}/${src}" "${dst}" "${hash}")
endforeach()

foreach(entry IN LISTS SIGIL_ASSET_URL_MANIFEST)
  string(REPLACE "|" ";" parts "${entry}")
  list(GET parts 0 url)
  list(GET parts 1 dst)
  list(GET parts 2 hash)
  _sigil_append_fetch("${url}" "${dst}" "${hash}")
endforeach()
file(APPEND "${_fetch_script}"
     "message(STATUS \"assets in \${out}\")\n")

add_custom_target(fetch_assets
  COMMAND ${CMAKE_COMMAND} -P "${_fetch_script}"
  COMMENT "Fetching demo assets into ${SIGIL_ASSET_DIR}"
  VERBATIM)
