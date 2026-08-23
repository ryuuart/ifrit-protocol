# Findings

Defects found while working. Each entry states what the code does, what it
was evidently intended to do, and what a test should assert once intent is
restored. Delete entries as they are fixed; delete the file when empty.

## The codepoint-substitution gate measures the wrong axis on vertical runs

`substituteGlyph` (compose `Paint.cpp`) admits a replacement only when its
advance matches the original's, probed via `SkFont::getWidths` — the
HORIZONTAL advances — regardless of the run's orientation. The gate's own
rationale is "the replacement is drawn at the original's pen position, so a
different advance would move every letter after it": in an upright vertical
run the pen steps by the VERTICAL advance, so that is the axis the verdict
should read. As written, a vertical column set in Hiragino Kaku Gothic ProN
refuses a kana-to-digit churn even though every glyph there advances exactly
one em down the column (their horizontal advances differ: 0.500 em vs
0.657 em), and would conversely admit a pair with equal horizontal but
unequal vertical advances, which really would shift the column below the
substitution. A refused-in-error swap leaves a cell horizontally
mis-centred at worst, not mis-flowed — so the current behaviour is
conservative for the common case but wrong on both edges.

A test should assert: on an upright vertical run, a substitution between
two glyphs of equal vertical advance and unequal horizontal advance is
honoured, and one with unequal vertical advances is refused — with the
horizontal run keeping today's horizontal verdict.
