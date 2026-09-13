# Findings

## psx_doom_fire renders a different plate after an unrelated rebuild

With floating-point contraction pinned off for every target of the tree,
the six scenes that used to move with every rebuild held byte-identical
across a rebuild whose only change was a comment line in a header every
sketch includes, and psx_doom_fire moved instead — a scene that had held
across every rebuild of the day before the flag. It steps a fire
simulation on a kept canvas for six seconds before its capture, seeded
from a fixed xorshift state, so any one-bit difference anywhere in the
step compounds over three hundred and sixty frames. What differs between
two builds of identical sources is not contraction now; the remaining
candidates are a read of a cell the step has not yet written, and code
in the sketch's own translation unit whose result the compiler may order
differently once inlining shifts. A test should build twice from
identical sources with one unrelated object changed between the builds
and require the same digest for this scene; the fix is in the sketch
once the read is found.

## sketch_test dies on WebCore's resource-usage thread after the shared engine shuts down

`shutdownSharedEngine()` destroys the one Ultralight renderer the process
booted, and `WebEngine::~WebEngine` releases it on the web thread once
the last view is gone. WebCore's `ResourceUsageThread`, started when a
page first loads, is not joined by that release: it keeps polling after
the renderer is freed and reads freed state, so about one run in three
of `sketch_test` dies with SIGSEGV or SIGBUS in whichever kit suite
follows the web-engine suites, and the crash report names that thread
rather than the test that was running. What the release evidently
intends is a process left as it was before the engine booted — which
the settled-page suite also assumes when it boots a second engine, and
finds it cannot. A test should shut the shared engine down, then boot a
view again and read a settled frame from it, with no thread of the
first engine left running; the fix is scry's — keep the one renderer
for the process's lifetime and hand it back to a later engine, or stop
that thread before the renderer goes.

## web_script moves under a four-job sweep and holds when rendered alone

In a sweep of the whole registry at four jobs, web_script's plate came
back with a different digest, and two renders of that one scene alone,
straight afterwards, matched the baseline byte for byte. The scene loads
a page through the shared web engine, whose frame arrives on a thread of
its own, so what the capture reads evidently depends on how far that
thread has run when the sweep is contended, which is a settle the sketch
does not wait for. A test should render the scene under load — the other
scenes of the sweep running beside it — and require the digest a solo
render gives; the fix is in the sketch or the settled-page door it
should be using, so the capture waits for the frame it describes.

## A text leaf's own padding grows its box and does not move its glyphs

`text(u8"12").font({.size = 10}).padding(8, 4)` lays out as a node 16 px
wider and 8 px taller than the line, and the glyphs are drawn at the
node's outer corner rather than inside that padding, so a fill on the
leaf — the scrim a reading stands on — hangs to the right of and below
the words instead of surrounding them. It is the same on a leaf in the
flow and on an absolutely placed one, so it is not the placement. What
the padding evidently intends is what it means on every other node and
in CSS: the content box inset by it, with the paragraph laid out and
drawn there. A test should place one text leaf with padding inside a
known box and require the first ink to stand one padding in from the
node's corner on both axes; the fix is in the kernel's text placement,
which positions the paragraph at the node's box rather than at its
content box. Until it lands, a component that wants air around a
reading puts the padding on a box AROUND the line — which is what
`compose::kit::cell` does for `Caption::reading`.

## cde_motif's Front Panel is drawn in a colour set nothing bound

Every piece of chrome on the desktop reads the ambient `cde::ColorSet`
through `cde::ambient()`, and the four `environment::Provide<ColorSet>`
scopes in the sketch cover the File Manager, its client area, its
scrollbar and its path field. Nothing covers the Front Panel: `frontPanel`,
`control`, `handle`, `panelSeparator`, `clockIcon`, `dateIcon`,
`workspaceSwitch`, `helpSubpanel` and `iconifiedWindow` are described
outside all four, so `ambient()` answers a default-constructed set whose
five colours are transparent black. `frontPanel` fills from `theme[2]`
explicitly and every icon carries its own `kIconColor`, which is why the
panel is visible at all — but its bevels, its handle texture, the date
page's month band and that band's word are painted in a transparent
colour and do not appear, and the panel does not change with the palette
the way the rest of the desktop does. The intent is stated in the header:
"the set this piece of chrome is being drawn in" is meant to be bound at
the scope that owns it, and dtsession's primary set is the one the panel
names in its own comment. A test should describe `frontPanel()` under a
bound set and require that its bevel's top-shadow pixel equals that set's
`ts`; the fix is one `Provide` at the top of `frontPanel()`, which will
move the plate.
