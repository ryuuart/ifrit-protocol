# Tonhalle posters

Two Swiss concert posters hang side by side on one wall, each kept as it
is drawn, each with a museum label under it.

## What it shows

- **The arc table**, left: Josef Müller-Brockmann's "beethoven" poster for
  the Tonhalle Zürich (1955), reconstructed rather than traced. Nine black
  arc runs on four rings whose widths double 1:2:4:8:16, every end landing
  on a multiple of the poster's 11.25° module, and the title and the
  two-column imprint in the clear white. The runs draw themselves on: each
  is a full circle outline starting at its run's start angle and revealed by
  a span mask, ring durations doubling from the inside out and every sweep
  linear, so the reveal is a geometric progression. It is complete before
  four seconds.
- **The collage**, right: a fictional concert poster after Wolfgang
  Weingart's "Kunstkredit Basel 1976/77" (1977), an offset lithograph in
  LACMA's collection — an asymmetric black ground, interrupted text strips,
  colliding scales, overlapping dot screens and diagonal framing round a
  vector diaphragm. Five clipped strips carry one headline and shift its
  printing register; a shaped run of lettering travels round the
  diaphragm; a red dot screen slides in multiply over the sheet. The concert
  programme and the diaphragm are an original adaptation, and the event and
  dates are fictional.

## How it is built

- Each plate is placed on the wall by its own box, both the same height.
  The arc table's fractions are fractions of its plate box; the collage is
  drawn on its own 1000 × 1400 sheet and scaled onto its plate with
  `scale()` about its top left corner, and clipped there as the print is.
- The arc runs are `kit::disc` outlines restarted with `shapes::arc`,
  stroked at the ring's width and masked with `by::spans(spans::upTo(...))`
  over a `motion::animate` mount transition; the imprint is a flex row per
  group with a fixed flush-right label column.
- The collage's printed sheet and its grain are `Cache::Texture` subtrees,
  so its moving pieces — the `textOnPath` orbit, the multiply screen and
  the five strips' offsets — move at paint without relayout.
- The faces are Helvetica Neue with Helvetica and Arial fallbacks.

## What to look at

- The arc table's reveal: the innermost ring first and fast, each ring
  outward taking twice as long.
- The collage's headline strips sliding out of register with each other
  while the lettering crawls round the diaphragm.

Reference and image credit for the collage:
[Wolfgang Weingart, Kunstkredit Basel 1976/77 — LACMA](https://collections.lacma.org/object/207622).
The museum image is used for visual comparison and is not bundled in the
sketch. The still is taken at 4.5 seconds.
