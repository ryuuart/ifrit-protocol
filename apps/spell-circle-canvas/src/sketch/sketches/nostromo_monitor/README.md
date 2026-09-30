# Nostromo service monitor

A film-interface study of the industrial computer language in *Alien*.
The display is a 1600 × 1000 native drawing. It includes the cabinet,
recessed glass, indicator banks, scored metal, a physical function-key
strip, a cooling-circuit plan, subsystem states, two measured traces,
five correlation channels, and a continuously arriving inquiry log.

The telemetry and arrangement are original. The study does not reproduce
a particular film frame, an authentic ship circuit, or engineering data.
The names identify the visual subject; the diagnostic story is a service
technician examining a delayed return-line actuator while the ship remains
in automatic flight.

## Reference observations

- [Ron Cobb's Alien gallery](https://www.roncobb.net/05-Alien.html) is a
  primary design source for the functional industrial vocabulary.
- [The contemporary Mediascene production account](https://www.gigerdb.com/articles/files/Mediascene_35_1979.pdf)
  describes a worn working ship, multiple screens with different
  technical roles, dense functional indicator banks, and hardware
  assembled from aircraft, automotive and radio equipment. Those
  production constraints guide the service-console story and material
  wear.
- [The original Mother console at Julien's Auctions](https://www.juliensauctions.com/en/items/107804/alien-1979-original-mu-th-ur-6000-mother-computer-console-with-dvd)
  identifies the production prop. The scored enclosure, amber lamp banks
  and physical key assignments in this study are authored hardware.
- [Mother's order readout reproduced by AvP Central](https://www.avpcentral.com/images/mother/special-order-937.webp)
  was visually inspected. Its green fixed-pitch capitals, generous line
  spacing and black field guide the inquiry's typography. The linked
  image is a film still reproduced by a secondary source, rather than
  a production-design document.
- [The recovered GRAM system](https://www.chilton-computing.org.uk/acl/htmls/gram/gram_paper.htm)
  describes computer readouts, maps and navigation graphics made for the
  Nostromo's monitors. The study's chart and diagram layers follow that
  functional approach; their geometry is authored here.
- [The docking-monitor still in that archive](https://www.chilton-computing.org.uk/acl/htmls/gram/refs/gram_aliena.jpg)
  was visually inspected. It combines paired thin lines, interrupted
  structural rings, filled rectangular accents, a timing readout and a
  faint background grid. Its blue-white display is a separate visual
  convention from Mother's green text terminal. This study combines the
  functional diagram grammar with a green service inquiry; it does not
  claim the cooling circuit appeared in the film.

The green inquiry display, amber physical lamps, fixed-pitch text and
meaningful subsystem labels guide the adaptation. The five-channel delay
analysis, cooling loop, warning colors, key assignments and cabinet
proportions are creative extrapolations.

## Craft exercised

The screen has a consistent information hierarchy: identity and access at
the top, system health at left, the primary network at center, measured
signals at right, and the machine's response beneath. Paired pressure
lines, arrows and hatched exchangers let the diagram read independently
of color. The delay markers join the warning to the analysis display.

The optical treatment is deliberately small. A library bloom creates the
near phosphor halo; one original shader bends the source coordinate,
modulates raster rows, adds deterministic grain and applies corner
falloff. The entire screen passes through the same tube. The metal and
keys remain outside that optical pass. No imported raster image is part
of the drawing.

The cabinet, diagram, grid and trace curves are retained texture leaves.
Signal points, scanning cursors, the command caret and status lamps are
bound independently to a single clock with different periods. Numeric
readings update every 0.6 scene seconds. Inquiry rows arrive every three
seconds through the feed kit; monotonic row identities preserve surviving
rows. Each row records its arrival time and binds the native hard typewriter
entrance's master progress to the scene clock. The full scene is described
once; only the readout and inquiry slots are replaced when their data changes.

## Render

From `apps/spell-circle-canvas`:

```sh
build/bin/Release/Sketchbook.app/Contents/MacOS/Sketchbook \
  src/sketch/sketches/nostromo_monitor/nostromo_monitor.cpp \
  --frame /tmp/nostromo-8-4.png --at 8.4 \
  --state /tmp/nostromo-study-state
```

The file `--frame` lane renders this Compose sketch through raster Skia,
including when `--gpu` initializes the device runtime. Use the bounded
file-window lane for Graphite validation. A later frame at 17.4 seconds
shows the log farther into its diagnostic cycle. Reload the sketch session
after editing `Tube.sksl`: the file is loaded through the resource hub, but
the direct runtime-program filter captures its compiled effect during setup.

## Authoring observations

These are requests for the library backlog, not claims of repaired defects.

**File-authored filters are supported by the high-level shader API.** An
empty-pixel `content` texture entry declares the executor-supplied input.
The ordinary shader constructor supplies typed uniforms, resource loading
and shader diagnostics; its source can participate in resource hot reload.
The texture-options name makes this route less obvious, but the header
documents it. Reading a rendered subtree needs no advanced recipe
construction. This sketch's direct runtime-program filter captures the
compiled effect and therefore needs a session reload to pick up shader edits.

**Material bindings do not pass through the filter conversion.** The
material conversion builds a filter from the source material's current
inputs. It is documented as a snapshot; re-description is needed to sample
live values again. The resulting filter also rejects `bind` because it has
no direct parameter program. The tube uses the public Skia compiler and
runtime-program filter, sets its typed values and binds the clock there.
That direct route was rendered with a changing beam and noise clock.
A binding-preserving material-to-filter conversion would keep file-authored
optical effects within the high-level vocabulary. A regression should bind
a material uniform, convert it, advance the live value and verify the
chosen live or snapshot contract; a separate case should assert that a
snapshot filter reports an unsupported binding clearly.

**True phosphor persistence needs a temporal source.** The stock filter
vocabulary supplies spatial bloom; its layer inputs describe the current
rendered layer. The graphics canvas can retain manual accumulation, so an
afterimage can be built through that immediate drawing path. A runtime-owned
history input for a retained subtree would let the text, diagrams and
telemetry share a real temporal decay without reimplementing their drawing.
A regression should flash a signal, remove it, assert that the residual
fades by scene time, and compare deterministic stepped captures. Resize and
reload should follow an explicit history policy. This is an API desire,
not a demonstrated rendering defect. The study currently has no simulated
history layer.

**The basic streaming and typewriter APIs were sufficient.** The feed kit
preserves rows by sequence, and the typewriter entrance gates shaped glyph
coverage. Its unit timing remaps an explicit master progress; the default
master is one, so mounting a row does not start an animation. Recording
the arrival time in each row and binding that progress to the native clock
allows new rows to print while surviving rows remain complete. No terminal
engine or substring animation was needed. The manual caret is a fixed
prompt marker; an eventual typing caret should use the existing text-unit
attachment seam and its track schedule rather than duplicate advances.

## Verification

Native file compilation and raster captures passed at 1.2, 3.05, 6.5 and 12
scene seconds. The 3.05-second capture shows the initial letter of the new
line; the 6.5-second capture shows the next line partially printed, with
surviving rows complete. Visual inspection checked the hardware surround,
transparent curved tube edges, typography, diagram boundaries, readout changes, independent
signal phases and inquiry scrolling. Shader output preserves premultiplied
source alpha so the optical pass cannot cover the unfiltered enclosure.
The material-to-filter snapshot rejected a live uniform binding as documented;
the direct runtime-program filter renders successfully without that warning.
These captures do not establish GPU execution. A separate bounded file-window
capture validates the Graphite lane on the Metal-capable host.
