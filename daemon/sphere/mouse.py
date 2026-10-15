"""Cursor position relative to the main display's center, as -1..1 on each axis."""
import sys


def cursor():
    if sys.platform != "darwin":
        return None  # TODO: Windows/Linux
    import Quartz

    pos = Quartz.CGEventGetLocation(Quartz.CGEventCreate(None))
    screen = Quartz.CGDisplayBounds(Quartz.CGMainDisplayID())
    w, h = screen.size.width / 2, screen.size.height / 2
    x = (pos.x - screen.origin.x - w) / w
    y = (pos.y - screen.origin.y - h) / h
    return max(-1.0, min(1.0, x)), max(-1.0, min(1.0, y))
