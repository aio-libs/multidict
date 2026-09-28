"""Objects that run a callback from their finalizer, to mutate a multidict
from inside one of its own methods.

A multidict drops references while it mutates itself: a replaced value, a
removed key, the items of an argument it has consumed. Whatever it drops
can run a finalizer, and so arbitrary Python code, at that point. Build
the multidict with these, switch the `Fuse` on around the call under test
and off again before looking at the result, so that setting up and
inspecting the multidict never fire them.

A finalizer runs promptly only where objects are reference counted, so
tests using these must skip PyPy.
"""

from collections.abc import Callable


class Fuse:
    """Runs `action` for a dropped object, while it is on."""

    def __init__(self, action: Callable[[], object]) -> None:
        self.action = action
        self.on = False
        self.fired = 0

    def fire(self) -> None:
        if self.on:
            self.fired += 1
            self.action()


class FinalizerValue:
    def __init__(self, fuse: Fuse, tag: str) -> None:
        self.fuse = fuse
        self.tag = tag

    def __del__(self) -> None:
        self.fuse.fire()

    def __repr__(self) -> str:
        return f"<{self.tag}>"


class FinalizerKey(str):
    fuse: Fuse

    def __del__(self) -> None:
        self.fuse.fire()


class FinalizerPair(tuple[str, object]):
    """A (key, value) item of an argument, dropped once it is consumed."""

    fuse: Fuse

    def __del__(self) -> None:
        self.fuse.fire()


def finalizer_key(fuse: Fuse, key: str) -> FinalizerKey:
    ret = FinalizerKey(key)
    ret.fuse = fuse
    return ret


def finalizer_pair(fuse: Fuse, key: str, value: object) -> FinalizerPair:
    ret = FinalizerPair((key, value))
    ret.fuse = fuse
    return ret
