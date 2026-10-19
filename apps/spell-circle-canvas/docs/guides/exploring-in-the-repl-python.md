# Explore a running sketch from a prompt, in Python

Saving a file is one way to change a sketch. A prompt is the other: type
a line, and the window shows what it did on the next frame. Nothing is
recompiled and nothing restarts, so a value you are tuning keeps its
place, a motion keeps its phase, and a mistake costs one traceback.

The prompt is your own interpreter. `import sigil` there, open a window,
and the sketch draws in the same process: its module's names, its
running instance and its context are ordinary variables at the prompt.

This guide opens `python_hello` — a circle swinging under a greeting —
and, without touching the file, changes its colour, adds a card, sets the
card moving, sets its text in another family, and takes a picture.

## Open a window in your interpreter

Run the file with Python's `-i`, which runs it and then leaves you at a
prompt among its names, and show its class in a window:

```sh
python -i apps/grimoire/sketches/python_hello.py
```

```pycon
>>> from sigil import live, weave
>>> window = live.open(size=(960, 640))
>>> hello = window.show(Hello)
```

`window.show` runs `Hello` as Grimoire runs it and answers the running
instance, which is also `window.sketch`. The window draws while the
prompt waits for a line, so the circle swings between keystrokes.

## Change a colour

The file reads `COLOR` every time it draws the circle, so assigning it is
the whole change:

```pycon
>>> COLOR
'#efb87e'
>>> COLOR = "#7fd1c7"
```

`python -i` ran the file as the module `__main__`, and the prompt is that
module: `COLOR`, `SPEED`, `Hello`, `kit` and `sin` are the file's names,
and `hello` is the running instance. The window draws on the thread the
prompt runs on, between your lines, so nothing changes under a line while
it runs.

## Add a card

A `draw` sketch draws through the method its class holds at each frame,
so a new method assigned on the class takes over at the next one. Keep
the old one, draw it, and draw a card over it. The card reads three
module names that the next steps change:

```pycon
>>> FAMILY = "Helvetica"
>>> BOB = 0
>>> def card(pen):
...     y = 40 + BOB * sin(pen.millis() / 350)
...     pen.fill("#1b2230")
...     pen.rect(404, y, 196, 104, 14)
...     pen.fill(COLOR)
...     pen.textFont(FAMILY, 30)
...     pen.text("A card", 426, y + 52)
...     pen.fill("#9aa4b2")
...     pen.textFont(FAMILY, 15)
...     pen.text("drawn from the prompt", 426, y + 82)
...
>>> drawn = Hello.draw
>>> def draw(self, pen):
...     drawn(self, pen)
...     card(pen)
...
>>> Hello.draw = draw
```

A blank line ends a block, as at Python's own prompt. `draw` looks
`card` up each time it runs, so redefining `card` alone changes the card
from the next frame on.

## Animate a value

The card's height on the canvas is a function of the clock, scaled by
`BOB`, which is zero. Give it a size and it bobs; give the circle a new
speed and it swings faster, because the file reads `SPEED` every frame
too:

```pycon
>>> BOB = 8
>>> SPEED = 3
```

## Set it in another family

`sigil.weave.fonts` is the catalogue of the type this program can set
in: the families the platform has and the faces a sketch's hub loaded.
It was imported beside `live`:

```pycon
>>> [name for name in weave.fonts.families() if name.startswith("Geo")]
['Georgia']
>>> FAMILY = "Georgia"
```

`weave.fonts.family(name)` answers one family with its styles and
variation axes, and `weave.fonts.find(name, style)` the face it names.

## Take a still

```pycon
>>> window.still("hello.png", density=2)
'hello.png'
```

The still is the sketch on screen as it stands, at twice its canvas,
with its clock untouched.

## When a line goes wrong

```pycon
>>> 1 / 0
Traceback (most recent call last):
  File "<stdin>", line 1, in <module>
ZeroDivisionError: division by zero
```

A raise at the prompt is the prompt's, and the sketch draws on as the
lines before it left it. A method that raises once it is drawing is
another matter: the sketch stops, as it does after a bad save, and
`window.errorLog` says why. Put the method back and run setup again:

```pycon
>>> Hello.draw = drawn
>>> window.restart()
```

`window.restart()` runs setup again on the same instance, so whatever
you assigned to it stands. `window.close()` takes the window away; so
does leaving the prompt.

## How it works

A window belongs to the thread that opened it, and on macOS that must be
the main thread, which is also the one reading your input. So the window
draws while the prompt waits. Python calls one hook, `PyOS_InputHook`,
whenever it waits for a line, at `python -i`, at its own prompt and at
`code.interact`; `live.open` installs it, and it handles the window's
events and draws its frames until a line is there to read. The frames are
drawn on that same thread, on Qt Quick's basic render loop, so the
sketch's Python never runs beside a line of yours.

Under IPython in a terminal, `live.open` registers the same wait as an
IPython input hook and enables it, the way `%gui` enables a toolkit's. In
a Jupyter kernel, a task on the kernel's event loop turns the window
between cells. A script that is not interactive keeps its window on screen
with `window.run()`, which returns once the window is closed.

`live.open(size, headless=True)` opens no window at all: each turn steps
the sketch and `still` photographs it, which is how a test or a machine
without a display explores. Qt is loaded the first time a window opens,
and never by an import.

## A prompt into a window elsewhere

When the sketch is drawing in another process — a Grimoire already open
on the file, or a kernel that is not the one you type into — a prompt
sends each input there instead:

```sh
sigil repl apps/grimoire/sketches/python_hello.py
```

With no Grimoire answering, this starts its window on the file and opens
a prompt into it:

```text
Python inside the running sketch: its module's names, `self` and `ctx`.
Each input runs between two frames; the window shows it on the next.
:reload imports the file again, :still path.png takes a picture, :quit leaves.
>>>
```

Each input runs in the sketch module's own namespace, beside the running
instance as `self` and its context as `ctx`, with every `sigil` module the
file did not import itself bound too, and the walk above reads the same
with `self` for `hello`. `:still hello.png` writes the still under
Grimoire's state root and copies it to the path; `:reload` imports the
file again exactly as saving it does, so every name the prompt assigned
goes with the old import; `:quit`, or the end of input, leaves, and a
window `sigil repl` started ends with it.

The prompt speaks to Grimoire through its protocol: each complete input
is one command, `session.evaluate`, carrying the text and whether it is
an expression, and the answer carries the value as `repr` writes it, what
the text printed and what it raised. The command runs **any** code a
client on this machine sends, with the sketch's whole reach over the
machine, so Grimoire keeps that door shut unless it is started with
`--repl`, and `sigil repl` starts it that way. To open the door on a
window you started yourself, give it the flag and point the prompt at it:

```sh
Grimoire apps/grimoire/sketches/python_hello.py --repl
sigil repl          # finds the window through its address file
```

`--to HOST:PORT` names an endpoint directly, and `--state DIRECTORY` the
state root of a Grimoire started with `--state`. In the window the
command runs on the render thread, after a frame is drawn and before the
next; a sketch that returns its tree from `setup` is asked for it again
after every input that ran to its end. A C++ sketch is compiled ahead of
time and evaluates nothing, so the prompt is refused there, saying so.

## Next

- [Write a sketch in Python](guide:a-sketch-in-python) — the file this
  prompt explores, written from the start.
- The Python package's README — `sigil.live` and its window, and
  `sigil repl` beside `sigil open` and `sigil render`.
- SigilProtocol's README — the evaluate door among the commands every
  host speaks.
