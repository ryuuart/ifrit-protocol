"""
me - this DAT.

frame - the current frame number.

An Execute DAT with Frame Start on runs onFrameStart once a frame. This
one reads the patch's cue — the levels from a CHOP, the colour from a
CHOP, the markers from a Table DAT — builds it into one Cue FlatBuffer
with the modules flatc generated from the cue_deck sketch's schema, and
sends it through a UDP Out DAT to the sketch, which listens on port
27025. The sketch draws the cue, Grimoire publishes the drawing over
Syphon, and a Syphon Spout In TOP brings it back into this patch.

The operators it reads, by name, beside this DAT:

    cue_levels   any CHOP; one channel per meter, named as the meter is
                 labelled, each from zero to one, first sample read
    cue_color    a Constant CHOP with channels r, g and b, each zero to one
    cue_markers  a Table DAT with a header row and columns name, beat
    cue_out      a UDP Out DAT: Network Address 127.0.0.1, Network Port 27025
    show_in      a Syphon Spout In TOP: Sender Name Cue Deck — not read
                 here; it is where the published drawing comes back

A CHOP Execute DAT watching cue_levels can call send_cue() from
onValueChange instead, which sends only when a level moves.
"""

import sys
from pathlib import Path

# THE GENERATED MODULES. flatc writes the CueDeck package beside the
# sketch's schema, and they are read where they stand, so a schema edited
# and regenerated is what the next frame sends. project.folder is the
# folder this .toe is saved in, one level below the repository root.
CUE_DECK = Path(project.folder).parent / "apps" / "grimoire" / "sketches" / "cue_deck"
if str(CUE_DECK) not in sys.path:
    sys.path.append(str(CUE_DECK))

import flatbuffers
from CueDeck import Channel, Cue, Marker
from CueDeck.Color import CreateColor

SHOW = "NIGHT GARDEN · ACT II"
TEMPO = 120.0  # beats a minute
BAR = 16.0  # the beats the sketch's ruler spans


def read_cue():
    """The patch's cue as plain values: what the operators hold this frame."""
    levels = op("cue_levels")
    color = op("cue_color")
    markers = op("cue_markers")
    return {
        "show": SHOW,
        "channels": [(channel.name, float(channel[0])) for channel in levels.chans()],
        "color": (float(color["r"]), float(color["g"]), float(color["b"])),
        "markers": [(row[0].val, float(row[1].val)) for row in markers.rows()[1:]],
        "beat": absTime.seconds * TEMPO / 60.0 % BAR,
    }


def encode(cue):
    """One Cue FlatBuffer. A builder writes back to front, so every string
    and every table a vector holds is made before the vector, and every
    vector before the table that holds it."""
    builder = flatbuffers.Builder(512)
    show = builder.CreateString(cue["show"])

    channels = []
    for name, level in cue["channels"]:
        label = builder.CreateString(name)
        Channel.Start(builder)
        Channel.AddName(builder, label)
        Channel.AddLevel(builder, level)
        channels.append(Channel.End(builder))
    Cue.StartChannelsVector(builder, len(channels))
    for channel in reversed(channels):
        builder.PrependUOffsetTRelative(channel)
    channel_vector = builder.EndVector()

    markers = []
    for name, beat in cue["markers"]:
        label = builder.CreateString(name)
        Marker.Start(builder)
        Marker.AddName(builder, label)
        Marker.AddBeat(builder, beat)
        markers.append(Marker.End(builder))
    Cue.StartMarkersVector(builder, len(markers))
    for marker in reversed(markers):
        builder.PrependUOffsetTRelative(marker)
    marker_vector = builder.EndVector()

    Cue.Start(builder)
    Cue.AddShow(builder, show)
    Cue.AddChannels(builder, channel_vector)
    Cue.AddColor(builder, CreateColor(builder, *cue["color"]))
    Cue.AddMarkers(builder, marker_vector)
    Cue.AddBeat(builder, cue["beat"])
    builder.Finish(Cue.End(builder))
    return bytes(builder.Output())


def send_cue():
    """This frame's cue, as one datagram."""
    op("cue_out").sendBytes(encode(read_cue()))


def onFrameStart(frame):
    send_cue()
    return


def onStart():
    return


def onCreate():
    return


def onExit():
    return


def onFrameEnd(frame):
    return


def onPlayStateChange(state):
    return


def onDeviceChange():
    return


def onProjectPreSave():
    return


def onProjectPostSave():
    return
