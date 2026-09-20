"""WHAT A MARKDOWN DOCUMENT CARRIES: every place in it a reader could
copy a name out of, and the short label a report names each document by.

Names are taken from the ```cpp blocks AND from the inline `code` spans,
because prose carries as many names as the blocks do.  A span standing
inside a bullet that opens with a header path is carried with those
headers, which is the one place a BARE name says what owns it.
"""

import os
import re

from .names import HEADER_PATH, LISTING_OPENER


def code_regions(path):
    """[(line, text, kind, headers)] — every ```cpp line and every inline
    `span`, each span carrying the header paths of the listing bullet it
    stands in (empty everywhere else)."""
    out = []
    fence = None
    listing = ()  # the headers of the bullet being read, if any
    for i, line in enumerate(open(path, encoding="utf-8").read().split("\n"), 1):
        if line.startswith("```"):
            fence = None if fence is not None else line[3:].strip()
            continue
        if fence is not None:
            if fence.startswith("cpp"):
                out.append((i, line, "block", ()))
            continue
        opener = LISTING_OPENER.match(line)
        if opener:
            listing = tuple(HEADER_PATH.findall(opener.group(1)))
        elif not line.strip() or re.match(r"^\s*[-*]\s", line) or line.startswith("#"):
            listing = ()  # the bullet ended
        for m in re.finditer(r"`([^`]+)`", line):
            out.append((i, m.group(1), "listing" if listing else "inline", listing))
    return out


def doc_labels(mds):
    """{path: a short name unique among them}.

    A library whose chapters are a nested README apiece — a kit's beside
    the page that links it — has more than one `README.md`, and a report
    that names them both the same cannot say which line it means.  So a
    label grows leftwards along the path until it is the only one.
    """

    def tail(md, n):
        return "/".join(os.path.normpath(md).split(os.sep)[-n:])

    labels = {}
    for md in mds:
        n = 1
        while n < len(os.path.normpath(md).split(os.sep)):
            if sum(1 for other in mds if tail(other, n) == tail(md, n)) == 1:
                break
            n += 1
        labels[md] = tail(md, n)
    return labels
