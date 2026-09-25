"""Native resource access, live byte feeds, recording and data output.

Sketches use ``ctx.assets.hub()``: the host advances recordings and owns
the session's feed leases. Standalone Python programs construct ``Hub()``;
``listen`` opens every linked transport on its first ask, and
``registerTransports`` is the explicit form. A feed's receive method never
waits; decode a message's owned payload on the calling Python thread.
"""
