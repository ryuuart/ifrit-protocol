# Layer stack

A picture built from named pixel layers, the way a layers panel builds
one: gilt rings inside a dial, three words baked from a compose tree and
squashed, a hatching that shows only inside the words, and a disc that
erases everything beneath it as it circles. The picture is one
`draw.Stack` made in `setup`; `draw` moves two placements and puts the
stack down with `pen.stack`. The canvas is 1200 × 800.

[A picture built from layers](../../../python/sigil/README.md#a-picture-built-from-layers)
in the Python README is the canon for every verb used here.

## The layers, bottom to top

| Layer | What it holds | How it meets the layers beneath it |
| --- | --- | --- |
| `rings` | Three rings stroked with one conic gradient, and a dial of ticks, drawn with the layer's own pen | `blend`, the default |
| `field` | `WORDS` as a compose tree, baked into the layer with `layer.bake` | `screen` at an opacity of 0.8, placed with unequal scale factors about the centre, so the words are squashed |
| `hatch` | Slanted lines across the whole canvas, drawn with the layer's own pen | masked by `field`, so it shows only where the words have ink, wherever the words' own placement puts them; its own placement slides it sideways a little each frame |
| `cut` | A white disc, drawn with the layer's own pen | `remove`: wherever it has ink it deletes the rings, the words and the hatching beneath it; its placement carries it round the centre |

Every layer is drawn or baked once, by `buildStack`, which `setup`
calls; it is the same lines a session at the prompt would type. A frame
changes only two placements, so the stack composites again from the
pixels it already holds and draws nothing on a layer. The panel at the
lower left is drawn by the frame's pen, outside the stack, and reads
each layer's blend, opacity and mask back from the stack, top first as a
layers panel lists them.

The capture is taken at 2.5 seconds, where the disc has bitten into all
three rings and into the words, so every layer shows.

## Edit it

| Name | What it changes |
| --- | --- |
| `RADII` | The rings' radii, in canvas units from the centre |
| `WORDS` | What the field layer bakes |
| `SQUASH` | The field's width and height factors |
| `ORBIT`, `ORBIT_SECONDS` | How far from the centre the disc circles, and how long one round takes |
| `HATCH_SPACING`, `HATCH_SPEED` | The gap between the hatching's lines, and how fast they slide |

## Run it

From this directory:

```sh
../../../spell-circle-canvas/build/bin/Release/Grimoire.app/Contents/MacOS/Grimoire python_layer_stack.py
```

Grimoire watches the file and runs `setup` again on every save, so the
stack is rebuilt from the edited calls.
