"""Native specimen pages and the stylesheet their stage sets."""

import warnings as _warnings

from _sigil.compose.kit import CaptionWhere as _CaptionWhere
from _sigil.sketch import kit as _native
from _sigil.weave import FaceSlant as _FaceSlant

from .._kit import children as _children
from .._kit import specification as _specification
from .._kit import stagedStyleSheet as _stagedStyleSheet


def houseTheme():
    """Return an editable copy of the builder of the house stylesheet."""
    return _native.houseTheme()


def studyTheme():
    """Return an editable copy of the builder of the study stylesheet."""
    return _native.studyTheme()


def featureTheme(density=Density.Compact):
    """Return an editable copy of the builder of the feature stylesheet,
    with compact or spacious margins."""
    return _native.featureTheme(density)


def darkTheme():
    """Return an editable copy of the builder of the dark stylesheet in the
    terminal voice, whose ``label`` register is an instrument's label: the
    terminal face at 12 px, tracked 2 px, in the ash."""
    return _native.darkTheme()


def houseFace(voice, weight=400, slant=None, *, italic=False):
    """The house face for a voice at a weight, resolved once and held;
    ``italic=True`` is ``slant=weave.FaceSlant.Italic``."""
    if slant is None:
        slant = _FaceSlant.Italic if italic else _FaceSlant.Upright
    return _native.houseFace(voice, weight, slant)


def stage(ctx, props=None, **properties):
    """Declare the native stage: the canvas, the moment, and the
    ``styleSheet`` that stands at the root of everything the sketch renders,
    over the house custom properties; a rule stands for a stylesheet of
    that one rule, and an omitted background is the ground the stylesheet
    states. The deprecated ``theme=`` and ``sheet=`` stage a builder's
    stylesheet with rules joined after it."""
    theme = properties.pop("theme", None)
    sheet = properties.pop("sheet", None)
    if theme is not None or sheet is not None:
        _warnings.warn(
            "theme= and sheet= give way to styleSheet=: a stock stylesheet "
            "such as kit.featureStyleSheet(), or a builder's styleSheet(), "
            "with rules joined by +",
            DeprecationWarning,
            stacklevel=2,
        )
    look = _stagedStyleSheet(properties.pop("styleSheet", None), theme, sheet)
    if look is not None:
        properties["styleSheet"] = look
    return _native.stage(ctx, _specification(Stage, props, properties))


def page(content, props=None, **properties):
    """Put content on a native page with themed title, subtitle and footer."""
    return _native.page(_specification(Page, props, properties), content)


def sectionHeader(props=None, **properties):
    """Announce a section with a label, a rule and a supporting note."""
    return _native.sectionHeader(_specification(SectionHeader, props, properties))


def section(content, props=None, **properties):
    """Place a section heading above content at the native theme's gap."""
    return _native.section(_specification(SectionHeader, props, properties), content)


def well(surface=None, props=None, **properties):
    """Apply the native well spec to a surface, or create an empty well."""
    plate = _specification(Well, props, properties)
    return _native.well(plate) if surface is None else _native.well(plate, surface)


def caption(body, *, label="", note="", measure=0, where=_CaptionWhere.Split):
    """Caption a body with a label and a note standing where ``where`` says,
    spaced by the sheet's caption gaps."""
    return _native.caption(measure, label, note, body, where)


def cell(picture, props=None, *, label="", note="", **properties):
    return _native.cell(_specification(Cell, props, properties), label, note, picture)


def cells(*children, props=None, **properties):
    run = _specification(Run, props, properties)
    if children:
        run.cells = list(_children(children))
    return _native.cells(run)


def panelGrid(*children, props=None, **properties):
    grid = _specification(PanelGrid, props, properties)
    if children:
        grid.cells = list(_children(children))
    return _native.panelGrid(grid)


def comparison(cases, props=None, **properties):
    """Align titles, controls, figures and notes with native comparison tracks."""
    spec = _specification(Comparison, props, properties)
    spec.cases = list(cases)
    return _native.comparison(spec)


def verdict(table, props=None, **properties):
    """Draw a measure CheckTable's rows, each verdict coloured by the sheet."""
    return _native.verdict(table, _specification(Verdict, props, properties))


def controls(surface, props=None, **properties):
    """Draw a Controls surface: a slider, toggle or choice per parameter."""
    return _native.controls(surface, _specification(ControlsView, props, properties))


def legend(*entries, props=None, **properties):
    """The key to a picture: each entry a colour and what it stands for."""
    key = _specification(Legend, props, properties)
    if entries:
        key.entries = list(entries)
    return _native.legend(key)


def swatchStrip(props=None, **properties):
    """A ramp's steps in order, each a patch, with its word under it."""
    return _native.swatchStrip(_specification(SwatchStrip, props, properties))


def chip(props=None, **properties):
    """One word set inside a patch."""
    return _native.chip(_specification(Chip, props, properties))


def meter(props=None, **properties):
    """A level as a bar along a track, with its label and reading."""
    return _native.meter(_specification(Meter, props, properties))


def gauge(props=None, **properties):
    """A level as an arc round a dial, with its reading in the middle."""
    return _native.gauge(_specification(Gauge, props, properties))


def backdrop(props=None, **properties):
    """The ground a scene stands on, with its vignette and grain."""
    return _native.backdrop(_specification(Backdrop, props, properties))


def panel(content, props=None, **properties):
    """A region of a sheet: eyebrow, title and note over a well of content."""
    return _native.panel(_specification(Panel, props, properties), content)


def frame(content=None, props=None, **properties):
    """A device's chrome round what its screen shows, or the chrome alone;
    `screen=` is the screen's own fill."""
    chrome = _specification(Frame, props, properties)
    return _native.frame(chrome) if content is None else _native.frame(chrome, content)


def scrollbar(props=None, **properties):
    """A scrollbar's arrows and thumb, sized from what it measures."""
    return _native.scrollbar(_specification(Scrollbar, props, properties))


def ticker(content, props=None, **properties):
    """A strip that runs one pass of its content past forever."""
    strip = _specification(Ticker, props, properties)
    strip.content = content
    return _native.ticker(strip)


def timeline(*marks, props=None, **properties):
    """A rail with its marks: ticks at fractions along it, and their words."""
    scale = _specification(Timeline, props, properties)
    if marks:
        scale.marks = list(marks)
    return _native.timeline(scale)


def titleCard(props=None, **properties):
    """The card a sequence opens on: eyebrow, title, subtitle and notes.
    A line may be given as its words alone."""
    for name in ("eyebrow", "title", "subtitle"):
        if isinstance(properties.get(name), str):
            properties[name] = Line(words=properties[name])
    if "notes" in properties:
        properties["notes"] = [
            Line(words=note) if isinstance(note, str) else note
            for note in properties["notes"]
        ]
    return _native.titleCard(_specification(TitleCard, props, properties))


def console(*feeds, props=None, **properties):
    """The log panel a study prints into, over the text rings given."""
    return _native.console(_specification(Console, props, properties), list(feeds))


def connectionReadout(connection, props=None, **properties):
    """A door's state as rows: where it is, whether it stands, its revision,
    what it dropped and what was no message, and who sent the newest."""
    return _native.connectionReadout(
        connection, _specification(ConnectionReadout, props, properties)
    )


def instrument(picture, readings, props=None, **properties):
    """A live picture beside its readings, on one page."""
    return _native.instrument(
        _specification(Instrument, props, properties), picture, readings
    )


def axis(props=None, **properties):
    """A plot's axis: its line, its ticks and their numbers."""
    return _native.axis(_specification(Ruler, props, properties))


def rules(props=None, **properties):
    """Hairlines across a plot's field at the domain values named."""
    return _native.rules(_specification(Rules, props, properties))


def trace(curve, props=None, **properties):
    """A curve across a plot's x domain: a function of one number, or a
    measured series spread across it."""
    return _native.trace(curve, _specification(Trace, props, properties))


def area(curve, props=None, **properties):
    """The band between a curve and a base."""
    return _native.area(curve, _specification(Area, props, properties))


def path(at, props=None, **properties):
    """A curve walked over a parameter in both coordinates."""
    return _native.path(at, _specification(Path, props, properties))


def label(words, x, y, props=None, **properties):
    """A word at a point of a plot's field."""
    return _native.label(words, x, y, _specification(Label, props, properties))
