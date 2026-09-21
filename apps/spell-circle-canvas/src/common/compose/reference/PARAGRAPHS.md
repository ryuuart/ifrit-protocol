# Paragraphs

A chapter of [TYPOGRAPHY.md](../TYPOGRAPHY.md), the type chapter of
[SigilCompose](../README.md).

## A passage whose input moves

**Settled text is the special case, not the moving kind.**
*`Element::textWillChange`
is a leaf saying that an input of its layout moves — a measure that
animates, a frame that grows, content that changes between frames — and
it buys two things: the break decisions of a block set in a uniform
measure are kept and reused, so a measure already crossed costs no
decision at all, and the block is broken against the measure alone rather
than against the frame's supply of lines, so a frame that changes only in
DEPTH changes which lines it holds and never where they break.

```cpp
text(caption, body).width(Dimension(measure)).textWillChange(true, 6000)
```

**NOTHING INFERS IT.** A live layout answers the overflow tail
differently from a settled one, so a guess would change the setting of a
page that never moves. A passage that moves says so.

The second argument is the floor under the frame, in BREAK CANDIDATES:
how many lines the optimizing breaker may score for one block before that
block is filled greedily for that frame. A degrade drops the whole
setting rather than the breaker alone — the hyphens, the justification
passes past the word gaps, and the widow rule go with it. It is a count
and never a stretch of clock, so whether a block is composed or filled is
a fact about its words and its measure and not a race with whatever else
the machine is doing. **A degrade is provisional.** The leaf does not
hold that layout as the answer for its measure, so the next frame lays
out again and the setting comes back the frame the floor is met.

`Composer::settling(key)` is what the frame actually got: `reused` blocks
answered from decisions already made, `degraded` blocks the floor forced
to the greedy breaker. It is a REPORT about one input and not a verdict
about the node — the runtime holds one proof that a node has settled, and
this is folded into it beside everything else the node reads. What the
proof takes from it is one bit: a live passage that still composed this
frame can be set differently the next one with no number on the node
moving, which no value memo can see, so it declares like a live material
does. A passage answered entirely from the store is set exactly as the
frame before it, and caches.

## Paragraphs, frames and stories

A hard break inside a passage separates **blocks** — the paragraphs a reader
sees — and `Element::paragraphStyles` says how each one is set, one entry per
block in block order:

```cpp
text(weave::rich(body).add(u8"A heading\nand its body, which runs on\nand on"))
    .width(Dimension(360.0f))
    .paragraphStyles({headingStyle, bodyStyle})
    .textFirstBaseline(sigil::weave::FrameOptions::FirstBaseline::kCapHeight)
    .textVerticalAlign(sigil::weave::FrameOptions::Distribute::kJustify);
```

`sigil::weave::ParagraphStyle` carries the leading, the air before and after,
the four indents, the keeps and whichever of the leaf's own alignment,
justification, hyphenation and tab stops the block overrides; SigilWeave's
README is the canon for what each one means. A block past the end of the list
is set in THE BLOCK IN FORCE where the leaf stands — the `weave::Block`
partials its ancestors declared through `Element::block`, folded down the tree
— so ONE entry styles the first block and leaves the rest to the passage.
`Element::paragraphStyles` with whole styles sets every block alike and
inherits nothing, and `Element::paragraphStyles` also takes NAMES, resolved
through the block half of the `sigil::weave::StyleSheet` in force where the
leaf lands into partials that are laid over the block in force when the leaf
lays out — the same discipline `weave::rich().add(text, name)` follows for
character styles. A name no sheet in force carries WARNS ONCE and changes
nothing about its block, because a block quietly set in a default nobody asked
for looks exactly like a style that did not take.

`Element::textFirstBaseline` and `Element::textVerticalAlign` are the two
decisions a FRAME makes that no line makes for itself: where baseline 0 sits
below the top of the box, and what becomes of the room left over down it.

**A story fills as many frames as it is given.** `weave::Story` is content plus
its block styles and nothing else — no layout, no cursor, no frame — and
`frame(story)` is one text leaf over it, which `Element::key` names and
`Element::thread` links to the next:

```cpp
weave::Story article(weave::rich(body).add(u8"…"));
article.paragraphStyles({headingStyle, bodyStyle, bodyStyle});

root.children({
    frame(article).key("a").thread("b").width(Dimension(300.0f)),
    frame(article).key("b").width(Dimension(300.0f)).textOverflow(u8"…"),
});
```

Each frame fills from where the one before it stopped, so the cut moves as
any frame's measure moves, and the blocks are numbered from the STORY's
start — the third block is set the same way whichever frame it lands in.

**A STORY NUMBERS ITS OWN LINES.** `weave::selectors::line(40)` is the fortieth line
of the story wherever it landed, so a chain that reflows moves the
selection with the text instead of addressing a different line in every
frame; words, characters, sentences and named runs were the story's
already, since every frame builds the whole story's paragraph and resumes
at a word. `selectors::inFrame("b")` is the frame-local address beside it —
everything the named frame holds, and nothing anywhere else — so
`selectors::inFrame("b") & weave::selectors::line(40)` is "line 40, if frame b is where it
landed". A frame-local address on a leaf with no `key` can never match and
warns once.

**BEATS SPAN THE CHAIN.** A cascade over a threaded story runs one clock
across the whole of it: with `beats::Text` the fortieth word is beat forty
wherever it landed, so a staggered reveal carries on from one frame into
the next instead of restarting, and a `fx::sequence` phase's crossfade stays
put across a reflow that moves a word from one frame to another —
its beat is the story's, not the frame's. The word, the sentence and the
line are the three granularities this holds for, because each carries a
story ordinal on the placed glyph. A CLUSTER AND A GLYPH DO NOT: their
ordinal is a position in this frame's walk, so a cascade over either
restarts at each frame.
Overflow on any frame but the last is the normal case and draws no marker,
whatever ellipsis the leaf asked for; the last frame is the one that
threads nowhere. A frame's own geometry is its business: it may flow
around a silhouette or carry exclusions like any other text leaf.
`kit::columns` is N frames side by side threaded in order, which is what a
Western multi-column measure is — the vertical writing mode keeps the word
column for the thing it already meant. It is spelled either positionally,
for a plain run of columns, or as a `kit::ColumnSet`, which is the same
run plus the things that straddle it. Both take an ellipsis, and it ends
the chain: the last column threads nowhere, so without one what
it cannot hold draws past its box, and the marker lands on that column's
last line instead. The columns before it take none whatever is passed,
because a mark at every cut would read as three texts rather than one
story.

The chain is walked in the derive pass, in chain order, with each frame
re-filled at the measure it resolved to before the next is asked what it
inherits — so a chain of any length settles in one pass rather than one
link per convergence round. A chain that closes on itself stops where it
closes, as a cyclic borrow does. The walk also tells each frame the
measure the frame AFTER it resolved to, which is the one fact the widow
rule needs and no single fill can see: the lines a widow rule counts are
the remainder, and the remainder is set in the next frame.

**A RUN OF THE CHAIN CAN BE BALANCED**, which is `Element::textThreadBalance`
on the frame that opens it: that frame and every one after it up to the
next frame that opens a run — or the chain's end — are filled to the
SHALLOWEST depth that still holds what the run was asked to hold, and all
of them resolve to that one depth. Three columns of one story then come
out three columns of the same length instead of two full ones and a stub.
The depth is found by halving, in the walk, with the depth the frames
DECLARE as the ceiling: a run whose frames are sized by anything but
pixels is left alone, and reading the resolved depth instead of the
declared one would halve the last round's answer every round until the
columns closed on nothing. The argument says what the run must hold, as a
story-relative line number; the default is all of it.

**A SPANNER BREAKS THE CHAIN**, and that is `column-span: all` stated in
the story's own terms. `kit::columns` takes a `kit::ColumnSet` with a list
of `kit::Spanner`s, each of which is a `weave::Selector` and an element:
the copy down to the unit the selector names sets in one balanced run of
columns, the element runs the full measure under it, and what is left
resumes in the next run. All of it is ONE story and ONE chain — the frames
below the spanner pick up the word the frames above ran out on — so a
selector, and never a y coordinate, is what says where the break falls.
`ColumnSet::height` is then the DEEPEST a row of columns may be rather
than the depth it will take, since every row above a spanner is balanced
down to what it must carry; the last row keeps the full depth, because
what follows it is the rest of the story.

Where each selector landed is read back from the layout the last draw
left standing, which is why `ColumnSet::composer` is passed, and on the
same terms as everything else here that reads a resolved layout: the
first draw has none, so it holds what it can and the spanners settle on
the draw after.
