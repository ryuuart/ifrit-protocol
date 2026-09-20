---
kind: verb
library: SigilCompose
name: tether
qualified: sigil::compose::Element::tether
header: sigilcompose/core/Element.h
group: Flow and placement
status: stable
---

# tether

Hang this node off a keyed one, at a stated pair of points, with a list
of places to try when the first will not fit. It takes the node out of
the flow, and where it lands is an answer of the layout rather than
something the description states.

## Syntax

```cpp
Element& tether(Tether t);
```

```cpp
tooltip().tether({.key = "port",
                  .on = {0.5f, 0.0f}, .at = {0.5f, 1.0f},
                  .offset = {0, -6},
                  .fallbacks = {{.key = "port",
                                 .on = {0.5f, 1.0f},
                                 .at = {0.5f, 0.0f},
                                 .offset = {0, 6}}}})
```

## Description

**Resolved after layout**, against the geometry the anchor resolved to,
and re-resolved whenever the anchor moves. `Tether` is where the value's
own rules live: what FITS means, and what an unknown key does.

**Every place the box may end up is declared as a read.** The anchor and
every fallback's anchor are nodes this one's answer is a function of, so
each is registered with the derive pass. A fallback that named a node
nothing waited for would be resolved a pass late, and the box would
flick into it a frame after the anchor moved.

**Last wins, and the previous tether's reads go with it.** A box hangs
off exactly one anchor at a time; one re-tethered would otherwise keep
waiting on every node it was ever tethered to.

**It is the node's own and not a property.** Registering what a node
reads off another is something only the node can do; a value that
merely stated where to hang would leave those reads unregistered.

## See also

`centerAt` for a point the description already knows, and `Tether` for
the value this takes.
