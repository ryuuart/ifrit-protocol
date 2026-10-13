# Guides

Task-shaped pages: one thing you want to do, start to finish, in both
languages. They are the way in. The
[value index](/values/index.html) is what you read afterwards, when a
signature asks for a value and you want to know what makes one.

Every guide runs against a **sketch** — one file that declares a scene,
which Grimoire opens and hot-swaps on every save. Nothing here needs
a window server or a network.

## Start here

0. **[Getting started: one sketch, from a box to a lit scene](guide:getting-started)**
   — the tour: a page, a row from data, motion, a pen, the same card as
   a scene in space, a material and a post pass, and where each library's
   own document is.
1. **[Put a coloured rounded box on the page](guide:a-coloured-rounded-box)**
   — a canvas, a box, a fill and a corner radius, and the two ways to
   run it.
2. **[Animate a fill](guide:animate-a-fill)** — make the colour move,
   three ways, and the one thing you must declare so a cache does not
   freeze it.
3. **[Write a sketch in Python](guide:a-sketch-in-python)** — the same
   API from the other language: install, render a still, open it live.
4. **[Compose elements as the material of a 3D body, in Python](guide:compose-driven-materials-python)**
   — a plaque whose face is a Compose page: materials on the fills, the
   page painted as the maps those materials name, worn by one quad under
   lights, with a pulse from the clock and an entrance the runtime owns.
5. **[An animated console in Python](guide:animated-console-python)**
   — a page that behaves like a website: a marquee and a pulsing alert as
   loops on lanes, typed lines and a meter as slots re-rendered from a
   timer, phosphor glow and scanlines over a cyberpunk console.
6. **[Many materials on one page, in Python](guide:mixed-materials-python)**
   — a specimen sheet worn by one quad: carved limestone type, brushed
   copper ribbons, a crackled glaze, velvet, a thin film, a glowing mark,
   satin and matte, each a fill whose material the scene lights.
7. **[A wire from TouchDesigner: JSON first, OSC for channels, a schema when it settles](guide:sigilio-flatbuffers-touchdesigner)**
   — a cue sent from a patch as JSON text, then as OSC from a CHOP with
   no script, then as a FlatBuffer through a schema once the message
   settles; one sketch reading all three as the same `data.Json`, a
   recording a still replays, and the canvas published back to the
   patch over Syphon.
8. **[A console driven from outside, in Python](guide:console-driven-from-outside-python)**
   — the WardNet console fed by JSON messages over UDP and a WebSocket
   that serves its own page of buttons: keyed rows that mount with an
   entrance, an alert chip and marquee that follow the newest alert, a
   meter eased on the engine and followed through a binding, a
   TouchDesigner script, and a recording a still replays.
9. **[Entrances and staggers in Python](guide:entrances-and-staggers-python)**
   — a grid of cards that opens from its centre on an eased stagger, a
   wipe along one axis of a grid, a seeded scatter, a queue that arrives
   row by row and leaves by reversing its playback, and a cursor that
   eases on change: what an entrance is and what a transition is.
10. **[A title sequence on a timeline, in Python](guide:timelines-python)**
    — labels, `at`, `afterPrevious` and `withPrevious`, a run that
    leaves on a stagger, calls that swap a credit into a slot, a
    transport timer that pauses, resumes and seeks, and why a still at
    `--at` is exact.
11. **[Live values and bindings in Python](guide:live-values-and-bindings-python)**
    — one number from an oscillator followed by a dial, a meter, a peak
    lamp and a stepped readout, each through its own binding: a range,
    an envelope, a wiggle and `quantize`; and the same panel fed from
    JSON arrivals.
12. **[Springs and oscillators in Python](guide:springs-and-oscillators-python)**
    — a panel that snaps between two places on a spring stepped from a
    timer, a colour that settles, `flash` and `decay` for a hit, three
    waves at one rate, and the three words for stillness.

## What to read next

- **The colour chapter on the [SigilCompose](doxygen:SigilCompose)
  site** — the four things that all mean "what colour is this", and
  which one a given slot takes.
- **[The elements](/reference/SigilCompose/elements/index.html)** —
  every factory that starts a tree.
- **[The verbs](/reference/SigilCompose/verbs/index.html)** —
  everything you can say to one, grouped by what it is about.
- **[The libraries](overview:libraries)** — what else is here.
- **[The glossary](glossary:)** — the house words.
