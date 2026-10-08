# Embossed foil

A sheet of metallised thermal blanket lies on a dark bench: gold foil
crinkled into a thousand small facets, quilted into soft pillows between
its stitches, hemmed in silver tape, with a word pressed into it by a die,
and one corner folded back to show the silver underside. A soft panel of
light travels behind it on a rail, so its reflection runs across the
blanket and every crease flashes and goes dark as the panel passes, while
the pressed letters, smoothed flat by the die, hold the panel as one clean
mirror among the crinkle; the silver corner lifts and settles on its fold,
catching the panel at its own angle and throwing a shadow across the gold.

## What it shows

- Crinkle that is only a normal map: the facets are a height field — fractal
  noise at three scales, drawn crease lines and long fold lines — turned into
  normals, so the panel's reflection breaks into sparks across a sheet whose
  real mesh is smooth.
- Pressed lettering as a deeper, flatter region of the same height field: the
  die erases the crinkle inside the letters and rolls a shoulder round them,
  so the word reads as a smooth mirror sunk into the foil.
- Foil over a fabric backing: a metal surface whose roughness follows the
  crinkle, with a faint sheen at grazing angles.
- A real body that is not flat: the blanket is a sheet displaced into quilted
  pillows and two broad folds, its corner cut away; the silver corner is a
  second, slightly curled body hinged on the cut, folding as the scene runs.
- A visible rect light on a rail with shadows, a cool fill, and a studio
  environment of soft boxes for the metals to reflect.

## How it is built

The crinkle is painted once into a height texture: `material::noise` in
turbulence and grain modes at three frequencies, `compose::brush::ribbon`
strokes for the creases and folds, stitch dimples, and the letter die
(`weave::TextContext` glyph outlines filled black), softened with
`material::Filter::blur`. `material::surface::normalFromHeight` turns it into
the normal map both foils share. Two `sketch::Page`s carry the gold and the
silver surfaces — `metallic`, a roughness masked by the crinkle, `sheenColor`
and `sheenRoughness` — and the hem tape and a two-line caption on the gold.
The blanket and the corner are `gm::grid` sheets with their UVs laid over the
page in page pixels. The light is `world::rectLight` inside
`world::kit::visibleLight`, placed on its rail and aimed at a null on the
blanket with `lookAt`, which turns the light and its panel as one; the fill is a second, cool `rectLight`; the room is
`world::kit::environmentMap` with soft boxes.

## What to look at

The panel's reflection crossing the gold: it is never one shape, but a field
of facets that light and die in turn. The pressed word, which holds a single
clean reflection while the foil round it sparkles. The silver corner as it
rises: its creases flash at a different moment from the gold beneath it, and
its shadow slides over the pillows.
