"""A sketch file rendered through the native host and read back."""

from pathlib import Path

from sigil import media
from sigil.sketch import renderFile


def renderedImage(source, output, *, at=0):
    """The frame @p source renders at @p at seconds, written to @p output
    and decoded back."""
    renderFile(source, output, at=at)
    return media.decode(media.Image, Path(output).read_bytes()).frameAt(0).image


def renderedPixels(source, output, *, at=0):
    """The RGBA bytes of the frame @p source renders at @p at seconds."""
    return renderedImage(source, output, at=at).rgba()


def pixel(picture, x, y):
    """The RGBA colour of @p picture at pixel @p x, @p y."""
    offset = 4 * (y * picture.width() + x)
    return tuple(picture.rgba()[offset : offset + 4])


def scaledPixel(picture, x, y, size, channels=4):
    """The colour at canvas point @p x, @p y of a canvas @p size wide and
    high, at whatever density @p picture was rendered at; the first
    @p channels of it."""
    column = x * picture.width() // size[0]
    row = y * picture.height() // size[1]
    offset = 4 * (row * picture.width() + column)
    return tuple(picture.rgba()[offset : offset + channels])
