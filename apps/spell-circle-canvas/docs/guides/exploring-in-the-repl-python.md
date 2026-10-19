# Explore a running sketch from a prompt, in Python

Saving a file is one way to change a sketch. A prompt is the other: type
a line, and it runs inside the sketch Grimoire is drawing, between two of
its frames, and the window shows what it did on the next one. Nothing is
recompiled and nothing restarts, so a value you are tuning keeps its
place, a motion keeps its phase, and a mistake costs one traceback.

This guide opens `python_hello` — a circle swinging under a greeting —
and, without touching the file, changes its colour, adds a card, sets the
card moving, sets its text in another family, and takes a picture.

## Open the sketch with the door open

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

The prompt speaks to Grimoire through its protocol, and the command it
sends runs **any** code a client on this machine sends, with the
sketch's whole reach over the machine. So Grimoire keeps that door shut
unless it is started with `--repl`, and `sigil repl` starts it that way.
To open the door on a window you started yourself, give it the flag and
point the prompt at it:

```sh
Grimoire apps/grimoire/sketches/python_hello.py --repl
sigil repl          # finds the window through its address file
```

`--to HOST:PORT` names an endpoint directly, and `--state DIRECTORY` the
state root of a Grimoire started with `--state`.

## Change a colour

The file reads `COLOR` every time it draws the circle, so assigning it is
the whole change:

```pycon
>>> COLOR
'#efb87e'
>>> COLOR = "#7fd1c7"
```

The input runs in the sketch module's own namespace: `COLOR`, `SPEED`,
`Hello`, `kit` and `sin` are the file's names, `self` is the running
`Hello`, and `ctx` the context its hooks are handed. An expression prints
its value; a statement prints only what it printed.

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
The prompt binds every `sigil` module the file did not import itself, so
`weave` is in reach:

```pycon
>>> [name for name in weave.fonts.families() if name.startswith("Geo")]
['Georgia']
>>> FAMILY = "Georgia"
```

`weave.fonts.family(name)` answers one family with its styles and
variation axes, and `weave.fonts.find(name, style)` the face it names.

## Take a still

```pycon
>>> :still hello.png
hello.png (1280 x 840)
```

The still is the sketch on screen as it stands, at twice its canvas,
with its clock untouched; Grimoire writes it under its state root and the
prompt copies it to the path.

## When a line goes wrong

```pycon
>>> 1 / 0
Traceback (most recent call last):
  File "<prompt 14>", line 1, in <module>
    1 / 0
    ~~^~~
ZeroDivisionError: division by zero
```

A raise is answered and changes nothing more: the sketch draws on as the
lines before it left it. A method that raises once it is drawing is
another matter — the sketch stops as it does after a bad save, and
Grimoire's window says why. `:reload` brings back the file as it is
written, which is also how to start over after exploring:

```pycon
>>> :reload
reloaded python_hello.py
```

It imports the file again exactly as saving it does, so every name the
prompt assigned goes with the old import. Anything worth keeping goes
into the file.

`:quit`, or the end of input, leaves; a window `sigil repl` started ends
with it.

## How it works

The prompt is Python's own `code.InteractiveConsole`: it gathers lines
until an input is complete, tells an expression from statements, and
keeps history through `readline`. Each complete input is one command of
the protocol, `session.evaluate`, carrying the text and whether it is an
expression; the answer carries the value as `repr` writes it, what the
text printed to the standard output and error, and what it raised with
its traceback.

In the window, the command is handed to the render thread, which runs
the sketch's own hooks, and runs there after a frame is drawn and before
the next; a headless host serving the protocol runs it on its frame
thread the same way. A sketch that returns its tree from `setup` is
asked for it again after every input that ran to its end, so a changed
name reaches the tree; a `draw` or a `describe` is asked at every frame
anyway. A C++ sketch is compiled ahead of time and evaluates nothing, so
the prompt is refused there, saying so.

## Next

- [Write a sketch in Python](guide:a-sketch-in-python) — the file this
  prompt explores, written from the start.
- The Python package's README — `sigil repl` beside `sigil open` and
  `sigil render`, and how Grimoire is found.
- SigilProtocol's README — the evaluate door among the commands every
  host speaks.
