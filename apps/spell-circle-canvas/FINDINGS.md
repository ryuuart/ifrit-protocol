# Findings

A work queue: each entry states what the code does, what it was evidently
intended to do, and what a test should assert once intent is restored.
Entries are deleted as they are fixed, and the file is deleted when it is
empty.

## live_settling's plate moves under a full sweep and holds alone

**What the code does.** In one whole-registry sweep, five jobs at once,
`live_settling` rendered with a different hash from its baseline; rendered
alone twice afterwards it was byte-identical to that baseline both times.
The sketch settles a web page through the settle machine, whose quiet
stage is a stretch of time with no repaint in it, and under the load of a
sweep the engine's repaints land later, so the frame the settle stops on
is not always the same frame.

**What it was intended to do.** A capture is a function of the
declaration alone; the settle's stages are judged on the page's own
events, and the quiet window was meant to be the one clock, long enough
that load does not move the frame it stops on.

**What a test should assert.** A settle on the same page, run under a
parallel load, stops on the same frame; the plate ledger's full sweep
and a single render of `live_settling` agree.
