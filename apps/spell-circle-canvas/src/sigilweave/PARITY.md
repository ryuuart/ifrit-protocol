# SigilWeave parity, control by control

The list a page-layout application's paragraph and character panels
present, and where each one stands here. It is the FEATURE list: what a
typesetter reaches for, not what a file format carries.

**From compose** is how a SigilCompose author reaches the same control,
because a control nobody can spell is a control nobody has. Every row
either names a verb, a field on a value a verb takes, or a kit item —
and the ones that name nothing say so. A verb spelled `Text::` is one
only the text leaf states: an `Element` cannot state it, and a `Text`
states everything an `Element` can. The paragraph's own fields reach a
tree as an `Element::paragraph` partial, a `ParagraphBlock` whose every
field is optional, inherited down the tree like a font.

| Control | Status | Where | From compose |
| --- | --- | --- | --- |
| Alignment: left / centre / right / justify variants | done, per block | `ParagraphStyle::alignment` | `Element::paragraph` (`ParagraphBlock::alignment`); per paragraph through `Text::paragraphStyles` |
| Justification: word spacing min/desired/max | done | `JustificationOptions::wordSpacing`, `spaceStretch`, `spaceShrink` | `Element::paragraph` (`ParagraphBlock::justification`) |
| Justification: letter spacing min/desired/max | done | `JustificationOptions::letterSpacing*` | `Element::paragraph` (`ParagraphBlock::justification`) |
| Justification: glyph scaling min/desired/max | done | `JustificationOptions::glyphScale*` | `Element::paragraph` (`ParagraphBlock::justification`) |
| Justification: single-word rule | done | `JustificationOptions::singleWord` | `Element::paragraph` (`ParagraphBlock::justification`) |
| Left / right / first-line / last-line indent | done | `IndentOptions` | `ParagraphStyle::indent`, through `Text::paragraphStyles` |
| Space before / after | done, larger-of | `ParagraphStyle::spaceBefore`, `spaceAfter` | through `Text::paragraphStyles` |
| Leading: auto, multiple, absolute, baseline grid | done | `Leading` | `ParagraphStyle::leading`, through `Text::paragraphStyles` |
| Leading: all above the line, or half above and half below | done | `ParagraphStyle::halfLeading` | through `Text::paragraphStyles` |
| Keep: widows, orphans, with next, all lines together, start in next frame | done — enforced at the frame boundary by retracting lines into the next fill, under both breakers | `KeepOptions` | `ParagraphStyle::keep`, through `Text::paragraphStyles` |
| Hyphenation: pattern dictionary | done, for any language that has a pattern table — the engine matches letters of any script and a table declares the language it answers for; the kit carries English and a caller loads the rest | `HyphenationOptions::patterns`, `kit::PatternHyphenator`, `kit::englishHyphenationPatterns` | `Element::paragraph` (`ParagraphBlock::hyphenation`) |
| Hyphenation: minimum word, letters before / after, capitalised words | done | `HyphenationLimits` | `Element::paragraph` (`ParagraphBlock::hyphenation`) |
| Hyphenation: consecutive limit, last word of a block | done | `HyphenationOptions` | `Element::paragraph` (`ParagraphBlock::hyphenation`); per paragraph through `Text::paragraphStyles` |
| Hyphenation: zone | done, both breakers | `HyphenationOptions::zone` | `Element::paragraph` (`ParagraphBlock::hyphenation`) |
| Composer: single-line vs paragraph | done | `LineBreakStrategy` | `Element::paragraph` (`ParagraphBlock::lineBreak`) |
| Composer: balance ragged lines | done — a bisection for the narrowest measure that keeps the line count, with the last line scored like every other. The narrowing is a FRACTION of each interval's own length, so a block an exclusion cut into unequal lines gives up the same proportion of every one of them; over a uniform measure the two searches coincide. ONE APPROXIMATION remains: the bisection stops after a fixed number of steps rather than at the exact fraction where the count turns over | `ParagraphStyle::balanceRaggedLines` | through `Text::paragraphStyles` |
| Composer: the live composer, and the floor under it | done — a moving input is declared, break decisions are kept per thread and reused at a measure already crossed, and a block the floor cannot finish is filled greedily for that frame and counted | `ParagraphLayoutOptions::live`, `KnuthPlassOptions::candidates`, `ParagraphLayout::reusedBlocks`, `degradedBlocks` | `Text::textWillChange`, reported by `Composer::settling` |
| Optical margin alignment (hanging punctuation) | done | `HangingTable`, `kit::hanging` | `Element::paragraph` (`ParagraphBlock::hanging`) |
| Initial letter: lines × graphemes | done — a block property, not a second element. The size is DERIVED: the initial's reference metric reaches from the first line's reference point to the baseline it sinks to, so a cap spans the lines it is given in any face. The notch is cut by wrapping the geometry, so exclusions, columns and contours all get it | `InitialLetter`, `initialLetterSize`, `ParagraphStyle::initial`, `ParagraphLayout::initial` | `Text::initialLetter` |
| Initial letter: sink and raise | done — `sink` is lines below the first baseline, negative raising it above; unset drops the initial to the last line it spans | `InitialLetter::sink` | `Text::initialLetter` |
| Initial letter: alphabetic / ideographic / hanging alignment | done — which reference metric the two alignments are made on: cap height, em box, or ascent | `InitialLetter::Align` | `Text::initialLetter` |
| Initial letter: wrap the glyph | done — the notch is the initial's own contours band by band, so a line tucks under the diagonal of an A | `InitialLetter::Wrap` | `Text::initialLetter` |
| Initial letter: an ORNAMENT instead of a letter | done as an ordinary exclusion — a keyed element with a flow shape, and a body that flows around that key | `ExclusionFlow` | `Element::key` with `Text::contentFlowAround` |
| Nested style | done as compose kit — the run stated in the text's own terms (words, a character count, or through a delimiter) and applied as a span restyle | `kit::NestedStyle`, `kit::nestedRun` | `kit::nestedRun` with `Text::span` |
| Bullets and numbering | done as compose kit | `kit::bullets` | `kit::bullets` |
| Tabs: position, leaders, alignment on a character | done | `TabStop` | `Element::paragraph` (`ParagraphBlock::tabStops`); per paragraph through `Text::paragraphStyles` |
| Paragraph rules above / below, shading | done as compose kit | `kit::rules` | `kit::rules` |
| Paragraph border | **not started** | — | — |
| Nested styles, GREP styles, line styles | exists | `selectors::regex`, `selectors::line`, span restyling | `Text::span` with a `SpanStyle` over the same selectors, plus compose's own `selectors::style` for a named run |
| Named character styles | exists — a base style and the named PARTIALS over it, whose lookup always answers: an entry states what it CHANGES, so one sheet serves a document whose base size was decided elsewhere | `TypeSheet`, `RichText::add(text, name)`, `RichText::styles` | `weave::rich().add(text, name)`, resolved through the `TypeSheet` Compose fills from the style rules in force, the run matched as a child of class `.name` |
| Named paragraph styles | exists in Compose, on the text leaf — the names resolve to the paragraph partials of the style rules in force | `ParagraphBlock`, `overlay` | `Text::paragraphStyles(names)`, resolved from the `paragraph` partials of the rules in force; a name no rule speaks about warns |
| A style stated in PART, inherited for the rest | done — every field of `Type` is optional, so a call site states the two it changes; it carries the whole inherited set, the shaping controls and the paint around the colour included; `overlay` resolves one partial against what it inherits from, `merge` folds two partials written about one passage into one, and `initialType` is what an unset field means when nothing is left above it | `Type`, `overlay`, `merge`, `initialType`, `toTextStyle`, `reshapes` | a `font` partial on an element, resolved against the style the tree hands down |
| A paragraph stated in PART, inherited for the rest | done — every field of `ParagraphBlock` is optional: leading, alignment, justification, hyphenation, tab stops, first- and last-line indent, widows and orphans, balanced ragging, writing mode and line-break locale, the fields a paragraph takes from the passage it stands in; `overlay` resolves one onto a whole `ParagraphStyle` and `merge` folds two into one | `ParagraphBlock`, `overlay`, `merge`, `toParagraphStyle`, `apply` | an `Element::paragraph` partial on any element, inherited down the tree; the names `Text::paragraphStyles` gives resolved over it |
| Character: a size stated against another size | done — px, em, rem or the line height, resolved where the number it is relative to is known; the face's own line height is `lineHeightOf` | `Length`, `em`, `rem`, `lh`, `lineHeightOf` | the same lengths on a partial |
| Character: size, tracking, horizontal scale | exists | `ShapingStyle` | the `TextStyle` a `text()` or `weave::rich()` run carries; `Text::span` with a `SpanStyle` stating `fontSize`, `letterSpacing` or a `font` partial with `Type::condense` |
| Character: metric kerning | exists (HarfBuzz) | shaping | the same style |
| Character: optical kerning | done, as a STATED APPROXIMATION: every adjacent pair of a word is measured — the narrowest distance between the left glyph's right edge and the right glyph's left, in bands off the outlines — and closed to the distance the FACE'S OWN even pair leaves, with the face's kerning table switched off because the two are answers to one question. A designer kerns by judging the white as an area and as a rhythm; this measures a distance, so a pair a designer would have opened for legibility comes out tighter. The library decides nothing about how tight type should be — the reference is the face's own — and no pair moves further than a stated bound | `ShapingStyle::opticalKerning` | the same field on the style a run carries; `Text::span` with a `SpanStyle` whose `font` partial states `Type::opticalKerning` |
| Character: baseline shift | done | `PaintStyle::baselineShift`, `Type::baselineShift` | `Text::span` with a `SpanStyle` whose `font` partial states `Type::baselineShift`, which never re-shapes |
| Character: skew | **not started** | — | — |
| OpenType features, small caps, figures, sets | exists | `kit/Features.h` | `ShapingStyle::fontFeatures` on the style a run carries, or `Type::features` inherited down a tree |
| Underline / strikethrough / overline / highlight options | exists | `Decoration`, `PaintStyle::decorations` | `Text::span` with a `SpanStyle` whose `font` partial states `Type::decorations` |
| Frame: columns, gutter | done as compose kit — a Western column is a FRAME | `kit::textColumns` | `kit::textColumns` |
| Frame: balance columns | done — a run of chained frames fills to the shallowest depth that holds what the run was asked to hold, so every frame in the run resolves to one depth; the frames must declare a pixel depth, since that declared depth is the ceiling the halving searches under. A spanner breaks the chain, and every row of columns above it is balanced down to what it must carry | compose, on the text leaf | `Text::textThreadBalance`; `kit::textColumns` with `kit::Spanner` |
| Frame: inset | exists | compose padding | `Element::padding` |
| Frame: vertical justification | done | `FrameOptions::distribute` | `Text::textVerticalAlign` |
| Frame: first-baseline offset | done | `FrameOptions::firstBaseline` | `Text::textFirstBaseline` |
| Frame: auto-size | exists | compose measure | a leaf given no width measures its own content |
| Threading (in and out ports) | done | `Story`; `layoutParagraph`'s resume word; the chain also states the next frame's measure through `ParagraphLayoutOptions::nextMeasure` | `weave::Story`, `frame`, `Element::key` and `Text::textThreadTo` |
| Story-wide addressing | done — a story's words, characters, sentences and named runs are the story's already, and the LINE is what a frame chain renumbers | `selectors::line` | `weave::selectors::line` addresses the story, compose's `selectors::inFrame` is the frame-local address beside it, and a cascade's beats span the chain on one master progress |
| Text wrap: bounding box, object shape, offsets | done — one `FlowShape` seam with a rectangle, a circle, an ellipse, any filled path, an image's alpha and a caller's own as peers | `FlowShape`, `Exclusion`, `flowshape::` | `Text::contentFlowAround`, over the target's own `Element::decorationOutline` |
| Text wrap: the standoff | done — `Exclusion::margin` is a DISC, so a diagonal edge stands off by exactly the margin and a corner rounds; measured off an exact Euclidean distance field where no analytic answer exists | `Exclusion::margin`, `image::distanceField` | the margin argument of `Text::contentFlowAround` |
| Text wrap: an image's own alpha, at a tolerance | done — inside where the alpha exceeds the threshold, so a soft edge admits words as the dial rises | `flowshape::coverage` | `Element::decorationOutline` over the target's coverage, with its threshold |
| Text wrap: jump object, wrap to one side | **not started** | — | — |
| Anchored objects: inline | exists | `Placeholder`, `RichText::slot` | `weave::rich().slot(name, size)` with a child keyed for that name |
| Anchored objects: above line | done, for a READING — a band reserved above the line and filled with set text; Compose's, on the text leaf | compose `Text::textAnnotation` | `Text::textAnnotation` |
| Anchored objects: custom position | done as compose kit — an object tied to a text unit and placed at an offset the caller states, with the x and y references named separately (the unit, its line, or the frame) | compose `kit::annotate` with `kit::Anchored` | `kit::annotate` with `kit::Anchored` |
| Room reserved beside every line | done — a layout input, in the strut before anything is broken | `ReservedBand`, `ParagraphStyle::reserved` | `Text::textLineMargin`; `Text::textAnnotation` reserves its own on top |
| Type on a path: orient, flip, start / end, align | exists; Compose's, on the text leaf | `PathFlow`, compose `TextPath` | `Text::textOnPath` |
| Type on a path: effects (skew, stair, gravity) | **not started** | — | — |
| Vertical writing (CJK columns) | exists | `WritingMode::kVerticalRL` on the Paragraph | `Element::paragraph` (`ParagraphBlock::writingMode`) |
| CJK: tate-chu-yoko | exists | `VerticalForm::kTateChuYoko` | the same field on a run's style; `Text::span` with a `SpanStyle` whose `font` partial states `Type::verticalForm` |
| CJK: ruby — mono, group, jukugo | done | `layout/Beside.h`; compose `Annotation`, `kit::ruby` | `Text::textAnnotation`, `kit::ruby` |
| CJK: kenten | done | `kit::kenten` | `Text::textAnnotation`, `kit::kenten` |
| CJK: kinsoku | done — ICU's own strict/loose tailoring under a locale, plus a table over the segmentation for a house's own additions; the stock table is derived from the line-break class each character carries, narrowed to the full-width cell | `Paragraph::setLineBreakLocale`, `KinsokuTable`, `kit::kinsoku` | `Element::paragraph` (`ParagraphBlock::lineBreakLocale`), `Element::paragraph` (`ParagraphBlock::kinsoku`) |
| CJK: burasagari | done — the hanging table, along the column | `HangingTable` | `Element::paragraph` (`ParagraphBlock::hanging`) |
| CJK: mojikumi (per-class spacing) | done, as a table over the gaps between words — the class of each character is the table's, whether a character is full-width at all is Unicode's | `MojikumiTable`, `ParagraphLayoutOptions::mojikumi` | `Element::paragraph` (`ParagraphBlock::mojikumi`) |
| CJK: tsume | done, as a fraction closed at every gap between two plain full-width characters. LIMIT: two characters shaped inside one word are set by the face and the shaper, and no fraction here moves them | `ParagraphLayoutOptions::tsume` | `Element::paragraph` (`ParagraphBlock::tsume`) |
| CJK: warichu | done — the note is cut where its two lines come closest in length and stacked inside the slot the base reserved | `warichuSplit`, `layoutWarichu` | **weave only.** The slot has to be sized from the note's own split, which is a question for a font context, and a compose description is written before there is one — so no verb reserves it yet. A caller holding a `FontContext` calls the two functions directly |
| Baseline grid | done | `Leading::grid` | `Leading::grid` through `Text::paragraphStyles` |
| Frame grid (CJK cell grid) | **not started** | — | — |
| What a decoration dresses | done — the node's shape, its glyph contours, or the outline it actually drew | — (a compose seam; weave supplies the glyph outlines) | `Element::decorationOutline` |
| Footnotes and endnotes | out of scope | — | — |
| Tables | out of scope — a layout, not a text | — | — |
| Text variables, cross-references, conditional text | out of scope — data, not typography | — | — |
