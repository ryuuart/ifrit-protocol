"""Values that change over time, the engine that runs them, and the arithmetic over a clock.

A property takes a number, colour or fill that stands still, a tween
describing how it moves, or a live value somebody writes. `animate()` makes
a described motion, `animatable()` a live value every copy shares, and
`bind()` follows a live number through the stages of a Binding. A length of
time is a number of seconds or a `datetime.timedelta`, and one read back is
a number of seconds. The sketch's engine is `ctx.engine`, the one clock its
animations, timelines and timers run on.
"""
