# Building and testing a library

From `apps/spell-circle-canvas`:

```sh
python3 scripts/sigil.py setup --config Release
cmake --build build --config Release --target <library>_test
ctest --test-dir build -C Release --output-on-failure
```

Each library has one GoogleTest binary, `<library>_test`, and one benchmark
binary, `<library>_bench`. Feature directories contribute sources and links
through `sigil_test()` and `sigil_bench()`. CTest discovers each native case
by name, so `ctest -R '^Suite\.'` selects a suite without another target.
Use Release for benchmarks and run them through `sigil.py bench`.

## Choose the smallest boundary

| Scope | Assert | Setup |
|---|---|---|
| Unit | A value, algorithm or ownership operation | Direct public values and deterministic inputs |
| Integration | Libraries working together, such as a Compose node driving a Draw pen | The library's raster fixture or a separately linked consumer |
| End to end | Registration, session, clock, rendering or module loading through a host | A native consumer fixture or the sketch host harness |

Keep pure cases free of host startup and rendering. Use one integration
case for a complete cross-library behavior instead of rebuilding the same
host setup in several unit cases. Add an end-to-end case when a product or
build boundary can fail even though its parts pass. Preserve assertions on
small contracts; a successful screenshot cannot establish resource release
or a dependency declaration.

A case names one promised behavior. Repeated claims with one input varying
use named parameters. Check values and layout relations directly. Raster
comparisons compare two renderings of the same input, or an adopted plate
under the platform and fonts it names. Timing belongs to benchmarks rather
than unit-test thresholds.

## Reuse the existing fixtures

- Compose's `SigilComposeTesting` provides `sigil::compose::test::Scene` in
  `sigilcompose/testing/Scene.h`: a composer, an explicit clock and a readable
  raster. Supply the font context, describe through its composer, step a
  frame and read pixels or an owned snapshot. Geometric checks remain in
  `sigilcompose/testing/Checks.h`.
- Draw's `test/support/Paper.h` provides a pen, raster and frame setup. It is
  also used by the Compose drawing-adapter cases. A caller may supply a
  different font context or none; individual pixel reads copy no surface.
- Sketch's `SigilSketchTesting` provides the in-process host without a test
  framework. `SigilSketchTestingHarness` adds the GoogleTest fixture in
  `sigilsketch/testing/Harness.h`, with `open`, `clock`, `step`, `still` and
  `compare`. Failed cases retain their artifacts and print the host's state.
- Native plugin integration configures a separate consumer, builds modules
  with CMake and loads the resulting artifacts through a native host. It
  verifies compiler/configuration and consumed-library compatibility at
  the loading boundary.

A helper needed by one file stays in that file. Shared fixtures live in the
owning library's `test/support/`; reusable consumer-facing verification
headers live under its testing target. Avoid a fixture hierarchy when a
plain value or one setup function is sufficient.

The tree's `src/test/` supplies `Fonts.h`, `ScratchDir.h`, `GlyphCanvas.h`
and `ShaderTable.h`. Instrument faces keep ordinary text tests independent
of installed fonts. Assets specific to a library live in `test/assets/`
and are reached through `SIGIL_TEST_ASSET_DIR`.

## Select a run

```sh
ctest --test-dir build -C Release -R '^Retained\.' --output-on-failure
ctest --test-dir build -C Release -L integration --output-on-failure
ctest --test-dir build -C Release -L e2e --output-on-failure
```

Scope labels are attached to the integration and host suites that declare
them; an unlabelled case does not by itself prove that it is a unit test.
Other labels state required facilities:

| Label | Required facility |
|---|---|
| `gpu` | Metal on Apple, or a Vulkan runtime |
| `fonts` | Installed platform fonts |
| `network` | An internet route |
| `oiio`, `ocio`, `svg`, `usd` | The named decode, color or scene backend |
| `substance`, `ultralight` | The named licensed SDK or its fixtures |
| `plates` | The adopted platform and font set |
| `cocoa`, `window` | A window server, and a real window for `window` |
| `protocol` | The protocol clients and host agents |

Use `ctest -LE <label>` to omit unavailable facilities. A skipped case is
reported as skipped coverage. A baseline adoption uses `sigil.py plates
--rebase` and records the intended visual change with the adopted images.

## Check consumer boundaries

An aggregate test binary collects every feature's link requirements. It
can conceal a missing dependency supplied by another feature. A public
header probe therefore compiles against its originating feature target,
and a consumer probe must link and run with only its stated requirements.
Compose's standalone native fixture, `sketch_compose_consumer`, covers its
basic Core drawing path without Sketchbook, Qt or another Compose feature
target. It is filed under Sketch's `cmake/test/native` and is built only
inside the separate native-library configure that
`SketchSDK.NativeLibraryBoundary` drives; that case is registered only when
`SIGIL_BUILD_APPS` is on, so a tree configured without the applications
never runs it.

`sigil_header_self_test()` compiles each exported header first and twice.
`sigil_doc_probes()` checks qualified API names and designated initializers
in each registered README and chapter. These guards establish header and
name consistency; they do not replace consumer link checks or executable
examples. The exact guard behavior is documented in
[the scripts guide](../../scripts/README.md).
