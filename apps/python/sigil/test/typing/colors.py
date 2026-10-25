"""One value of every colour, fill and surface-paint spelling, at one
representative parameter of each role.

Three unions name the three roles a colouring parameter can have, and
what each promises is that every member reaches every parameter written
with it. This file is the regression that keeps them from drifting
apart: it is type-checked as an authoring file, and it is run, so a
spelling the declarations allow and the binding refuses fails here, as
does one the binding takes and the declarations reject.

Every member is written out rather than looped over, because a list of
floats standing in a sequence is inferred as `list[float]`, which is not
the `list[FloatLike]` a colour declares, and the loop would test the
inference rather than the union.
"""

from typing import assert_type

from sigil import compose, material, motion

# ---------------------------------------------------------------------------
# ColorLike — a flat colour value, at material.Paint.solid.

assert_type(material.Paint.solid(material.Color("#6e99bb")), material.Paint)
assert_type(material.Paint.solid("#6e99bb"), material.Paint)
assert_type(material.Paint.solid((0.43, 0.6, 0.73)), material.Paint)
assert_type(material.Paint.solid((0.43, 0.6, 0.73, 1.0)), material.Paint)
assert_type(material.Paint.solid([0.43, 0.6, 0.73, 1.0]), material.Paint)

# Every spelling reads the same four floats, and a colour reads back as
# the colour class, so a colour taken off one value passes into the next.
written = [
    compose.Fill.color(material.Color("#6e99bb")).colorValue,
    compose.Fill.color("#6e99bb").colorValue,
    compose.Fill.color((0.43, 0.6, 0.73)).colorValue,
    compose.Fill.color((0.43, 0.6, 0.73, 1.0)).colorValue,
    compose.Fill.color([0.43, 0.6, 0.73, 1.0]).colorValue,
]
assert_type(written[0], material.Color)
assert all(abs(read.r - 0.43) < 0.01 and abs(read.a - 1.0) < 0.01 for read in written)
assert_type(material.Paint.solid(written[0]), material.Paint)

# The constructors named after CSS's functions each answer the colour
# class, and a channel may be a number or percentage text.
assert_type(material.Color.rgb(110, 153, 187), material.Color)
assert_type(material.Color.srgb(0.43, 0.6, 0.73, alpha=0.5), material.Color)
assert_type(material.Color.hsl(210, "40%", 60), material.Color)
assert_type(material.Color.oklch(0.7, 0.1, 240), material.Color)
assert_type(material.Color.hex(0x6E99BB), material.Color)
assert_type(material.Color.mix("#6e99bb", (0.1, 0.2, 0.3), 0.25, space="oklch"), material.Color)
assert material.Color.rgb(110, 153, 187) == material.Color("rgb(110 153 187)")
assert_type(material.Color("#6e99bb").css(), str)

# ---------------------------------------------------------------------------
# FillLike — a colour, a paint, a material or a reference, at compose.Fill.

accent = compose.var("accent")
solid = material.Paint.solid("#b34a2f")
assert_type(compose.Fill(material.Color("#6e99bb")), compose.Fill)
assert_type(compose.Fill("#6e99bb"), compose.Fill)
assert_type(compose.Fill((0.43, 0.6, 0.73)), compose.Fill)
assert_type(compose.Fill((0.43, 0.6, 0.73, 1.0)), compose.Fill)
assert_type(compose.Fill([0.43, 0.6, 0.73, 1.0]), compose.Fill)
assert_type(compose.Fill(compose.Fill.currentInk()), compose.Fill)
assert_type(compose.Fill(accent), compose.Fill)
assert_type(compose.Fill(solid), compose.Fill)
assert_type(compose.Fill(None), compose.Fill)
assert compose.Fill(None) == compose.Fill.none()
# CSS's two words for a colour the tree resolves are fills, as text.
assert compose.Fill("currentcolor") == compose.Fill.currentInk()
assert compose.Fill("var(--accent)") == compose.Fill.var(accent)
# A fill compares as its paint does, so the same paint written twice is
# one fill.
assert compose.Fill(solid) == compose.Fill(material.Paint.solid("#b34a2f"))
# The glyph OUTLINE is one fill on the node.
assert_type(compose.text("words").textStroke(1, "#6e99bb"), compose.Text)
assert_type(
    compose.text("words").textStroke(1, compose.Fill.currentInk()), compose.Text
)
assert_type(compose.text("words").textStroke(1, accent), compose.Text)
assert_type(compose.text("words").textStroke(1, solid), compose.Text)
assert_type(compose.text("words").textStroke(1, None), compose.Text)

# A custom property holds a colour, a length or a material, set one at a
# time or as the defaults a subtree reads.
brushed = material.Material("#b87333")
assert_type(compose.box().var("ground", "#18252e"), compose.Element)
assert_type(compose.box().var("inset", 12), compose.Element)
assert_type(compose.box().var("metal", brushed), compose.Element)
assert_type(
    compose.box().varDefaults({"ground": "#18252e", "inset": 12, "metal": brushed}),
    compose.Element,
)

# ---------------------------------------------------------------------------
# MotionFillLike — a fill that may move, at Element.fill; the other
# surface verbs take FillLike.

ramp = material.Paint.linearGradient(
    (0, 0),
    (40, 0),
    [(0, "#fff"), (1, "#123")],
    material.GradientOptions(units=material.GradientUnits.Pixels),
)
recipe = material.surface.unlit(material.surface.SurfaceParameters(baseColor="#e75a31"))
bound = motion.animatable(compose.Fill.color("#6e99bb"))
moving = motion.animate(from_=compose.Fill.none(), to=compose.Fill.color("#6e99bb"))
tinting = motion.ColorTween(from_="#000000", to="#6e99bb")
fading = motion.animate(from_="#000000", to="#6e99bb", duration=0.3)
assert_type(fading, motion.ColorAnimatable)
assert_type(compose.box().fill(fading), compose.Element)
assert_type(compose.box().fill(material.Color("#6e99bb")), compose.Element)
assert_type(compose.box().fill("#6e99bb"), compose.Element)
assert_type(compose.box().fill((0.43, 0.6, 0.73)), compose.Element)
assert_type(compose.box().fill((0.43, 0.6, 0.73, 1.0)), compose.Element)
assert_type(compose.box().fill([0.43, 0.6, 0.73, 1.0]), compose.Element)
assert_type(compose.box().fill(compose.Fill.currentInk()), compose.Element)
assert_type(compose.box().fill(accent), compose.Element)
assert_type(compose.box().fill(solid), compose.Element)
assert_type(compose.box().fill(None), compose.Element)
assert_type(compose.box().fill(compose.Fill(ramp)), compose.Element)
assert_type(compose.box().fill(ramp), compose.Element)
assert_type(compose.box().fill(recipe), compose.Element)
assert_type(compose.box().fill(bound), compose.Element)
assert_type(compose.box().fill(moving), compose.Element)
assert_type(compose.box().fill(tinting), compose.Element)

# The surface verbs that are not Element.fill, on the widest member each
# of them could have been narrowed against: a recipe instance.
assert_type(compose.Fill(recipe), compose.Fill)
assert_type(compose.stroke(1, recipe), compose.PathFormat)
assert_type(compose.text("words").ink(recipe), compose.Text)
assert_type(compose.kit.dot((0, 0), 2, recipe), compose.Element)
assert_type(compose.kit.line(fill=recipe), compose.Element)
assert_type(compose.kit.ladder(count=2, pitch=8, fill=ramp), compose.Element)
assert compose.Fill(None) == compose.Fill.none()
assert compose.Fill(recipe) != compose.Fill.none()
# A material is one kind of paint, so a paint slot takes one too.
program = material.Paint.sksl(
    "uniform shader source; half4 main(float2 p) { return source.eval(p); }"
)
assert_type(program.slot("source", recipe), material.Paint)

# ---------------------------------------------------------------------------
# ElementInkLike — deliberately narrower: no animatable, because a bound
# ink would make every node that inherits it volatile.

assert_type(compose.box().ink(material.Color("#6e99bb")), compose.Element)
assert_type(compose.box().ink("#6e99bb"), compose.Element)
assert_type(compose.box().ink((0.43, 0.6, 0.73)), compose.Element)
assert_type(compose.box().ink((0.43, 0.6, 0.73, 1.0)), compose.Element)
assert_type(compose.box().ink([0.43, 0.6, 0.73, 1.0]), compose.Element)
assert_type(compose.box().ink(accent), compose.Element)
