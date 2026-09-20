# SigilWeave — the Unicode analysis

The chapter on the leaf everything else in this library stands on: the
Unicode text analysis a layout engine needs, as plain values over UTF-16
text. `README.md` beside the library is the front page; the document
model that consumes these answers is `reference/PARAGRAPHS.md`.

## What the leaf is

Transcoding, locale-aware case mapping, script itemization,
bidirectional levels, line, word and sentence segmentation, and the
per-character properties that decide where a run may break, which glyphs
inherit their neighbours' typeface, and how a character stands in a
vertical column. Every function takes text or a code point and returns
its answer; nothing here holds state a caller can see, and nothing here
knows about fonts, styles, or a canvas.

Positions are UTF-16 code-unit offsets, as everywhere else in the
library. Scratch objects the analyses reuse — break iterators, the bidi
analyzer — are thread-local, so every function is safe to call from any
thread with no shared state.

## Character properties

`unicode::isWhitespace` is whitespace in the sense that separates words:
the space separators other than the no-break spaces (U+00A0, U+2007,
U+202F), plus the control characters that separate lines, fields and
paragraphs (TAB through CR, U+001C through U+001F, NEL, LINE SEPARATOR,
PARAGRAPH SEPARATOR).

`unicode::isHardLineBreak` is whether one UTF-16 unit forces a line break
after itself — the four line-break classes that break unconditionally:
the mandatory-break characters (VT, FF, LINE SEPARATOR, PARAGRAPH
SEPARATOR), CR, LF and NEL. It is a question about the CHARACTER; whether
a given BOUNDARY is mandatory is `unicode::LineBreak::mandatory`, which
is the segmentation's answer and covers the sequences (CR LF) a character
cannot.

`unicode::inheritsTypeface` is whether a code point never triggers a
typeface switch and instead takes the typeface of the run it sits in:
joiners, variation selectors, whitespace, controls and format
characters, and every combining mark.

`unicode::mayRequireBidi` is whether a code point can force
right-to-left directionality: its bidirectional class is one of the
right-to-left ones, the Arabic letters included. Text with no such code
point resolves to one uniform left-to-right level without a full
bidirectional pass. It is a property lookup rather than a range test, so
a script Unicode adds tomorrow cannot be missed by it.

`unicode::isLetter` is general category L, which is every script's
letters and includes the ones that have no case at all.
`unicode::isUpperCase` is general category Lu; title case (Lt) is not
upper case, because a digraph whose first letter alone is capitalised is
a letter of its own.

`unicode::isFullWidth` is whether a code point is set in a FULL-WIDTH
CELL — East Asian Width Wide or Fullwidth. That is the property behind
every question this engine asks about "ideographic" text: a full-width
character has no spaces around it and the zero-width gap beside it is
what a justified CJK line spends its slack on. It is a character property
and not a script one, so fullwidth Latin (Ａ Ｂ Ｃ) set among kanji
answers the same way the kanji do.

`unicode::verticalOrientation` returns how a character stands in a
vertical column (UTR#50): upright as in CJK, rotated a quarter turn as in
Latin, or upright with a substitute glyph the font supplies for the
vertical form.

## Scripts

`unicode::Script` carries ICU's `UScriptCode` enumerators as an integer
so no ICU header is needed to hold one; `unicode::kCommonScript` and
`unicode::kInheritedScript` are the two codes that name no script of
their own.

`unicode::ShaperScript` is A SHAPER'S SCRIPT TAG: the four-letter ISO
15924 code packed one letter per byte, which is the form a shaping engine
is told a run's script in. It too is carried as an integer so no shaping
header is needed to hold one. `unicode::shaperScript` maps a script code
to it; Common, Inherited and any code outside the valid range answer with
the tag under which a shaper applies its default rules, which is what
text belonging to no particular script wants.

`unicode::itemize` splits text into script runs. Common and Inherited
characters attach to the preceding specific script, or to the following
one when they open the text. Text with no specific script at all is one
`unicode::kCommonScript` run, and empty text yields one empty run ending
at 0. The overload taking caller-owned storage clears it first, so a
caller re-itemizing every frame amortizes its allocation away.

## Line-break classes

`unicode::lineStartProhibited` is every code point whose UAX#14
LINE-BREAK CLASS is one a line may not BEGIN with, ascending: the closing
punctuation and the closing parentheses, the non-starters, the
conditional Japanese starters, the exclamation and question marks, and
the infix numeric separators.

This is the CLASS a character carries, over the whole of Unicode, and not
where a given text actually breaks — that is `unicode::lineBreaks`, which
resolves the classes against each other under its tailoring and already
forbids most of what is listed here. A caller wanting a prohibition set
narrows this to the characters it means: a convention about full-width
punctuation makes no claim about ASCII.

`unicode::lineEndProhibited` is every code point whose class is one a
line may not END with: the opening punctuation, which would otherwise
close a line with nothing to open. It is a class listing under the same
terms.

## Case mapping

`unicode::Case` names the three full Unicode mappings, so the result may
be longer than the input ("ß" uppercases to "SS"). `unicode::caseMap`
writes into caller storage under a BCP 47 locale ("tr" makes "i"
uppercase to "İ"); empty means the process default locale, and a refusal
by ICU returns false with the output unspecified. `unicode::caseMapped`
returns the mapped text, or the text unchanged when the mapping fails.

`unicode::lowerCased` is the SIMPLE lower-case form of ONE code point:
one code point in, one out, under no locale. It is the mapping to match
text against a table with, because the result stands where its input
stood and an offset into it still names the character it came from —
where `unicode::caseMap`'s full mapping may lengthen the text and lose
that correspondence. A code point with no lower-case form is returned
unchanged.

## Segmentation

`unicode::LineBreak` is ONE LINE-BREAK OPPORTUNITY: where a line may end,
and whether it MUST. The offset is one past the unit the break follows.
`unicode::LineBreak::mandatory` is the segmentation's own answer — the
rule that opened the boundary was one of UAX#14's unconditional ones — so
a CR LF pair, which no per-character test can judge, is one mandatory
break and not two.

`unicode::lineBreaks` returns every line-break opportunity in the text
(UAX#14), ascending: the offsets a line may end at, each one past the
unit it follows, always ending with the text's size and never containing
0. Empty text yields one opportunity at 0. A break after a soft hyphen
(U+00AD) is reported like any other.

Its locale selects the tailoring the segmentation runs under: a BCP 47
tag, optionally carrying ICU's line-break keyword (`"ja@lb=strict"` sets
the strict Japanese rules a printed page uses, `"zh@lb=loose"` the loose
ones), and empty is the untailored root behaviour every text gets by
default. Iterators are cached per locale, so alternating between two
costs no more than staying in one.

`unicode::graphemeBoundaries` returns every GRAPHEME CLUSTER boundary in
the text (UAX#29), ascending, starting with 0 and ending with the text's
size; the clusters are the ranges between consecutive entries. A cluster
is what a reader calls one character, so it is the unit anything cutting
text apart must land on: a combining mark, a Hangul syllable, a
regional-indicator pair and an emoji ZWJ sequence are each indivisible
here. Empty text yields `{0}`.

`unicode::wordBoundaries` returns every word boundary (UAX#29), with
punctuation and spaces included as words of their own.
`unicode::sentenceStarts` returns the offset where each sentence starts,
the first entry always 0; the sentence containing an offset is the last
entry not greater than it.

## Bidirectional analysis

`unicode::BaseDirection` is the paragraph direction an analysis resolves
against, and `unicode::BidiRun` a maximal run at one embedding level —
odd levels read right to left. `unicode::bidi` resolves the text's
embedding levels (UAX#9) into runs that cover it from 0 to the text's
size with no gaps. Text whose direction is uniform is one run at level 0
or 1, empty text yields no runs, and a failed analysis yields one run at
level 0.

## See also

`reference/PARAGRAPHS.md` for the document these answers are stored on,
and `reference/LAYOUT.md` for the breakers that spend them.
