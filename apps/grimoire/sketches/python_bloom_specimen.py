"""One optical bloom filter with its coupled controls stated together: the
same subject under each starting configuration, each cell captioned with
the whole parameter set, for fine luminous type and for broad emissive
surfaces, and one row where the near and the broad halo each stand alone
because the other's strength is zero.

EDIT THESE FIRST
  FINE_TYPE — the starting configurations for fine luminous type.
  BROAD_SURFACE — the starting configurations for a broad emissive surface.
  HALO_BASE — the configuration whose two halos are shown apart.

TAGS: Python, Materials/Optics, Typography/Interface
"""

from sigil.compose import Element, box, column, document, text
from sigil.material import BloomOptions, Filter
from sigil.sketch import kit, sketch

NAME = "Bloom specimen"
CATEGORY = "Python"

SIZE = (1280, 1110)
MEASURE = 1180
FIGURE = (264, 132)
WELL_GROUND = "#05070a"
LUMINOUS_INK = "#9fe6ff"
LAMP = "#ffb35c"
LAMP_LETTERING = "#2a1606"
MONOSPACE = "Menlo, SF Mono, monospace"
FINE_WORDS = "0417 9328 5562"
FINE_SIZE = 22
FINE_TRACKING = 2
THEME = kit.featureTheme(kit.Density.Compact)

# Each configuration is the whole parameter set, stated in BloomOptions'
# own field names, so a value read from a caption can be typed back here.
FINE_TYPE = {
    "Tight": dict(
        sigma=0.9, strength=0.4, spread=3.6, tail=0.2,
        threshold=0.2, knee=0.12, softness=0.2,
        whitening=0.1, dilation=0.1, deepening=0.7, maximumOpacity=0.65,
    ),
    "Even": dict(
        sigma=1.6, strength=0.6, spread=4.0, tail=0.35,
        threshold=0.25, knee=0.2, softness=0.0,
        whitening=0.25, dilation=0.4, deepening=0.4, maximumOpacity=0.65,
    ),
    "Wide": dict(
        sigma=2.4, strength=0.45, spread=6.0, tail=0.6,
        threshold=0.3, knee=0.3, softness=0.0,
        whitening=0.15, dilation=0.8, deepening=0.9, maximumOpacity=0.65,
    ),
}
BROAD_SURFACE = {
    "Soft": dict(
        sigma=5.0, strength=0.5, spread=4.0, tail=0.5,
        threshold=0.35, knee=0.3, softness=0.0,
        whitening=0.15, dilation=0.0, deepening=0.3, maximumOpacity=0.65,
    ),
    "Deep": dict(
        sigma=7.0, strength=0.7, spread=4.5, tail=0.9,
        threshold=0.3, knee=0.4, softness=0.0,
        whitening=0.3, dilation=3.0, deepening=0.7, maximumOpacity=0.5,
    ),
}
HALO_BASE = dict(
    sigma=1.8, strength=1.0, spread=7.0, tail=1.0,
    threshold=0.25, knee=0.25, softness=0.0,
    whitening=0.2, dilation=0.4, deepening=0.5, maximumOpacity=0.65,
)
HALOS = {
    "Both halos": HALO_BASE,
    "Near halo alone": {**HALO_BASE, "tail": 0.0},
    "Broad halo alone": {**HALO_BASE, "strength": 0.0},
}


def optics(configuration: dict | None) -> Filter:
    """The bloom a configuration states; none is the unfiltered layer."""
    if configuration is None:
        return Filter()
    return Filter.bloom(BloomOptions(**configuration))


def parameters(configuration: dict | None) -> str:
    """The whole parameter set, one coupled group to a line."""
    if configuration is None:
        return "no filter\nthe subject as it is drawn"
    values = configuration
    return "\n".join(
        (
            f"extract  threshold {values['threshold']:.2f}  knee {values['knee']:.2f}",
            f"source   softness {values['softness']:.2f}",
            f"near     sigma {values['sigma']:.1f}  strength {values['strength']:.2f}",
            f"broad    spread {values['spread']:.1f}  tail {values['tail']:.2f}",
            f"core     whitening {values['whitening']:.2f}  dilation {values['dilation']:.1f}",
            f"halo     deepening {values['deepening']:.2f}",
            f"cover    maximum opacity {values['maximumOpacity']:.2f}",
        )
    )


def fine_type(configuration: dict | None, key: str) -> Element:
    """Fine luminous type: a line of small monospaced figures in a cool
    light ink, the filter on the text leaf itself."""
    return (
        text(FINE_WORDS, size=FINE_SIZE, color=LUMINOUS_INK)
        .fontFamily(MONOSPACE)
        .letterSpacing(FINE_TRACKING)
        .filter(optics(configuration))
        .key(key)
    )


def broad_surface(configuration: dict | None, key: str) -> Element:
    """A broad emissive surface: a lit lozenge with dark lettering, which
    the halo's maximum opacity keeps legible."""
    lettering = text("OPEN", size=20, color=LAMP_LETTERING).letterSpacing(6)
    return (
        box(lettering)
        .width(132)
        .height(44)
        .borderRadius(22)
        .alignItems("center")
        .justifyContent("center")
        .fill(LAMP)
        .filter(optics(configuration))
        .key(key)
    )


def figure(subject: Element) -> Element:
    width, height = FIGURE
    return kit.well(
        subject,
        width=width,
        height=height,
        ground=WELL_GROUND,
        corners=6,
        content=kit.WellContent(),
    )


def part(eyebrow: str, cases: list[tuple[str, dict | None, Element, str]]) -> Element:
    comparison = kit.comparison(
        [
            kit.ComparisonCase(
                title=title,
                control=parameters(configuration),
                figure=figure(subject),
                note=note,
            )
            for title, configuration, subject, note in cases
        ],
        measure=MEASURE,
        gap=24,
    )
    return column(document.eyebrow(eyebrow), comparison).gap(14).flexShrink(0)


def page() -> Element:
    fine = part(
        "01 / FINE LUMINOUS TYPE",
        [
            ("REFERENCE", None, fine_type(None, "fine.Reference"),
             "The ink before any light is extracted from it."),
        ]
        + [
            (name.upper(), values, fine_type(values, f"fine.{name}"), note)
            for (name, values), note in zip(
                FINE_TYPE.items(),
                (
                    "A close halo that keeps every counter open.",
                    "A lit core and a halo one stroke wide.",
                    "A long tail that sinks toward the ink's strongest channel.",
                ),
            )
        ],
    )
    broad = part(
        "02 / BROAD EMISSIVE SURFACES",
        [
            ("REFERENCE", None, broad_surface(None, "broad.Reference"),
             "The surface before any light is extracted from it."),
        ]
        + [
            (name.upper(), values, broad_surface(values, f"broad.{name}"), note)
            for (name, values), note in zip(
                BROAD_SURFACE.items(),
                (
                    "A soft spill; the cover cap keeps the lettering readable.",
                    "A dilated body of light with a deep, long tail.",
                ),
            )
        ],
    )
    halos = part(
        "03 / THE TWO HALOS, EACH WITH THE OTHER'S STRENGTH AT ZERO",
        [
            (name.upper(), values, fine_type(values, f"halo.{name}"), note)
            for (name, values), note in zip(
                HALOS.items(),
                (
                    "Both weights as stated: the near halo inside the broad one.",
                    "Tail zero: the near Gaussian alone, hugging each stroke.",
                    "Strength zero: the broad Gaussian alone, sigma times spread.",
                ),
            )
        ],
    )
    return kit.page(
        column(fine, broad, halos).gap(30),
        title="Bloom specimen",
        subtitle="One optical bloom filter / extraction, two halos and the core, stated together",
        footer="Edit a configuration's numbers; every cell is Filter.bloom over the layer it is attached to.",
    )


@sketch(size=SIZE, captureAt=0.0, theme=THEME)
class BloomSpecimen:
    def setup(self) -> Element:
        return page()
