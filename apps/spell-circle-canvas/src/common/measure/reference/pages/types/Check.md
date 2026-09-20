---
kind: type
library: SigilMeasure
name: Check
qualified: sigil::measure::Check
group: Checks
status: stable
---

# Check

## Description

One claim, its evidence, and its verdict.

The point is that the printed line is COMPUTED from the same two values
it reports. A hand-formatted caption saying "RING GEOMETRY EXACT" reads
identically whether the geometry is exact or not, because the sentence
and the measurement are joined only by whoever typed them; here they
cannot drift apart. And because the verdict is a value rather than a
string, a set of them can fail a build — see
`sigil::measure::failures`.

### What a row's verdict means to the run it stands in

A verification is not only claims. Beside them stand the measurements
the claims were made from, the titles that group them, and claims about
the SUBJECT rather than the construction — a published formula that does
not hold, a plate whose legend its engraving contradicts — whose failing
is the finding and not a defect in the run. Each is a row, and what it
means is stated by `sigil::measure::Standing` rather than typed into its
text, so a reader of the table and a build that counts its failures read
the same thing.

| Standing | What it is |
| --- | --- |
| `Standing::Claim` | a claim about the construction: its FAIL fails the run |
| `Standing::Finding` | a claim about the subject: its verdict is printed as any claim's is and never counted against the run |
| `Standing::Reading` | a measurement reported beside the claims and judged by nobody: its value is printed with no verdict |
| `Standing::Heading` | a title over the rows that follow: a label alone |

`sigil::measure::finding` restates a claim as a finding —
`finding(check("legend holds", 1.0, measured, 0.01))` is how a plate is
shown to contradict its own legend. `sigil::measure::reading` reports a
value under a label with no verdict: a residual in scientific notation,
a count, a ratio, a name. `sigil::measure::heading` is a title over the
rows that follow it.

`sigil::measure::failures` counts the CLAIMS that failed — an exit code
for a verification run, and what makes the claims mean something away
from the screen. A finding that fails is not among them: its failing is
a statement about the subject, and `sigil::measure::findings` counts
those.

### The line a row prints as

`Check::line` writes `  <label padded> <actual, right-aligned>   PASS`,
or `… FAIL want <expected>` — the shape of `"  %-44s %8ld   %s"`. Values
right-align because a column of results is a table, and a ragged number
column is hard to scan at small type.

The `want` clause matters: a failure that prints only the computed
number says something is wrong without saying what would have been
right, which a reader cannot act on.

Long labels are NOT truncated — they push the value column right
instead. A clipped label silently loses the units or the qualifier at
the end of a claim, which is worse than a line that wraps.

A reading stops after its value, since it has no verdict to print; a
heading is its label alone, unindented, so it reads as the title of the
indented rows under it.

### The overloads that make one

The integral overload of `sigil::measure::check` is the conservation
check, where two counts must agree exactly. It is constrained to
integral types on purpose: a plain `long` parameter would swallow
`check("r", 257.972, measured)` through an implicit truncation and
report EXACT on two numbers that differ; requiring integers makes that a
compile error and sends the caller to the tolerance overload, which is
the only correct way to compare floats.

The tolerance overload has no default tolerance on purpose: how closely
a measured value and a solved one must agree is a property of the
construction being checked, and an epsilon picked in the header would be
a claim the header is not entitled to make.

The text overload is for a claim whose evidence is a name or a spelling
rather than a number — byte comparison, no trimming, no case folding —
and the bare assertion is for a claim with no two numbers to compare
("every interior arc endpoint has degree 2").

### The table a run prints as

`sigil::measure::CheckTable` holds the checks in the order they were
made and prints them as one table: every row through `Check::line` at a
shared width, then a summary row, so a run of claims reads as a column
and ends with its verdict. `CheckTable::add` appends a row and answers
the table, so rows chain; `CheckTable::failures`,
`CheckTable::findings`, `CheckTable::pass` and `CheckTable::checks`
read the verdict off the rows without printing them.

`CheckTable::lines` answers one string per row, then a final
`  <n> checks, <m> failed` line (`all passed` when none did), with
`, 1 finding` or `, <k> findings` after it when a finding did not hold —
the noun agrees with the count, since a summary that says "1 findings"
reads as a defect in the report rather than in what was reported. The
readings and headings are printed and not counted. It is empty when
there are no rows: a table with nothing in it prints nothing rather than
a summary of nothing.
