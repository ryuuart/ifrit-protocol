# Glossary

The house words, and which library owns each one. A word listed against
a library is that library's to define; where two libraries genuinely use
the same word for two things, both are stated, because guessing wrong
about those two is the commonest way to read a signature backwards.

| Word | Owner | What it means |
| --- | --- | --- |
| **brush** | SigilDraw, SigilCompose | A line vocabulary. In SigilDraw a brush is natural media over the pen — a marker, a nib, a bristle. In SigilCompose the brush tier composes decorations into strands along one route. |
| **cascade** | SigilCompose | The five things that flow down the tree from a node to everything under it: the font a passage is set in, the ink, the block its paragraphs are set in, the sheet its classes resolve through, and the custom properties. Everything else a node says stays on that node — CSS's own split, and the rule of thumb transfers: text properties inherit, box properties do not. |
| **composer** | SigilCompose | The object that holds the retained tree, takes a description, reconciles it and paints. |
| **connection** | SigilData | A named live source read as data — the front door a drawing binds to, over a feed. |
| **decoration** | SigilCompose | A value with a paint entry point, attached to a node with `background`, `foreground`, `overlay` or `stroke`. It reads only the outline it is handed, so the same value dresses geometry you built yourself. |
| **derivation** | SigilCompose | Anything that cannot be resolved until a neighbour's box exists — a connector between two keyed nodes, a rail, a thread, text flowing around a target. Each declares what it reads, and the derive phase resolves them in that order. |
| **describe** | SigilCore | Building the description: a value-typed tree, made fresh and thrown away, that says what the frame should look like. Not draw calls. |
| **element** | SigilCompose | Two things, deliberately: the node value the whole library is built on, and — in this reference — the kind of page for a factory that starts a tree. A library with no node type has no elements; SigilDraw's equivalent is a pen verb. |
| **feed** | SigilIO | A named stream on the hub: messages arriving on a wire, delivered to whoever asked, recordable and replayable. |
| **fill** | SigilCompose | What one surface is painted with, as the reconciler stores it: nothing, a colour, a shader, or a reference the tree resolves where the mark lands. |
| **gate** | SigilCompose | What a mask reads to decide coverage — an alpha or luma source, and the parts it applies to. |
| **hub** | SigilIO | The runtime resource and message centre: URIs mount onto directories and transports, results are cached, and a poll re-stats what has been loaded so edited files reload without a restart. |
| **ink** | SigilCompose | The colour in force at a node, inherited by everything under it. A mark that names no colour is painted in it, and `Fill::currentInk()` is the spelling for a slot that demands a value. It is CSS's `currentColor`. |
| **kit** | every library | A stock value over a seam: a composed component, a named ramp, a ready-made sheet. A kit composes and never decides — anything it does, a caller could have written with the library's own vocabulary. |
| **lane** | SigilCompose, SigilGeometry, SigilMeasure | A named channel of values carried beside others: a transform lane on a node, a named attribute lane on a point cloud, one of the three lanes a frame timer feeds. |
| **leaf** | SigilMaterial, SigilCompose | In SigilMaterial, what fills a recipe slot when it is an image and its sampling rather than another material — bound by the backend rather than compiled. In SigilCompose, a node with content and no children, of which a text leaf is the common one. |
| **material** | SigilMaterial | One instance of a recipe: the field values as upload bytes, the live bindings, the materials filling its slots, and the instance settings. One KIND of paint, not the top of the colouring lattice. |
| **memo** | SigilCore | The skip that avoids a describe whose inputs compare equal to last frame's. |
| **paint** | SigilMaterial | What a surface is shaded with, as a comparable value that compiles to one shader. Distinct from Skia's own draw paint, which carries a style, a stroke width and a blend mode for one draw. |
| **pen** | SigilDraw | The immediate-mode drawing object: p5's verbs with p5's names, argument orders and defaults, over a canvas. |
| **plate** | SigilSketch | A rendered still of a sketch, taken headless and compared against the recorded one. The plate ledger is what judges a change: a moved plate is a fact to be explained, not a failure. |
| **primitive** | every library | Something irreducible, which lives in a library's core. If a consumer needs one that does not exist, the library that owns that domain grows — the consumer does not invent its own. |
| **promotion** | SigilCompose | Turning a node's cached picture into a texture, decided by a policy the painting composer runs under. Never something a promotion is allowed to do — only what decides one. |
| **prune** | SigilCore | Skipping the repaint of a node whose description compares equal to the retained one. Everything in the tree that compares by value exists so that this is cheap and provable. |
| **recipe** | SigilMaterial | A material's definition: a struct of uniform-typed fields that is its ABI, one shader body per language, the slots it samples, and the per-frame values it reads. |
| **reconciler** | SigilCore | The kernel that matches a fresh description onto the retained tree — by key, then by position — so only what changed is touched. |
| **seam** | every library | The boundary a thing is stated across: one CPU executor, and a device executor beside it serving the same seam. A value over a seam is a kit value; the seam itself is core. |
| **sheet** | SigilCompose, SigilSketch | In SigilCompose, the stylesheet a node's classes resolve through, which is one of the five things that cascade. In the sketch kit, the specimen sheet a study is laid out on: a title, a subtitle, a footer and a run of captioned cells. |
| **sketch** | SigilSketch | One file — or one directory named for that file — that declares a scene, addressed by its stem in one registry. Everything renderable in this repository is one, in C++ or in Python. |
| **span** | SigilCompose, SigilWeave | In SigilCompose, which runs of a boundary a pass claims, so a stroke draws along part of an outline. In SigilWeave, a styled run of text inside a paragraph. |
| **strand** | SigilCompose | One member of a composite brush: where it runs and what paints it, as one value rather than two index-matched lists. |
| **surface paint** | SigilCompose | The widest colouring value: a fill, a fill that moves, a material paint, or a recipe instance. Every other colouring value converts into it. |
| **track** | SigilWeave | One kinetic effect over a run of units, with the units being the glyphs, words or lines it addresses. |
| **value** | every library | What you CREATE to pass to a verb. The reference gives each one a page saying how to make one, what takes it, and what returns it. |
| **verb** | SigilCompose, SigilDraw | Something you SAY to a node or a pen: a member that returns it, so calls chain. A verb naming a CSS-inherited property writes that lane and inherits. |
| **volatility** | SigilCompose, SigilMaterial | Whether a value repaints differently from one frame to the next. Every value that can answer it spells it `isAnimated`, always derived from how the value was built and never a setter. A value that repaints and does not declare it is frozen by the first cache that sees it, with no error and no warning. |
| **wire** | SigilSeer, SigilIO | One URI a message arrives on or leaves by. Seer is the tool that watches all of them at once. |

## Words about the build, not the libraries

| Word | Where | What it means |
| --- | --- | --- |
| **probe** | `cmake/Sigil.cmake` | The compile check over a document's prose: every qualified API name a README or a chapter spells must exist in a header, or the build fails. Headers win. |
| **ledger** | `scripts/sigil.py` | A recorded set of numbers a run is compared against — plates, benchmarks, coverage. A ledger owns numbers so that comments and documents do not. |
| **sweep** | Sketchbook | A headless pass over the whole sketch registry, rendering each one. A file outside the registry is photographed on its own and never enters it. |
| **tier** | plates, paints | A level a thing is judged or resolved at: the plate ledger's device and promotion tiers, and a paint's static, geometry and live tiers. |
