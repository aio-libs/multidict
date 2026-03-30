from multidict import MultiDict
from contextlib import suppress


class BadItem:
    """Looks like a 2-element sequence but __getitem__ raises."""

    def __len__(self) -> int:
        return 2

    def __getitem__(self, i: int) -> object:
        raise RuntimeError("intentional getitem failure")


if __name__ == "__main__":
    # This should not segfault, if it does then this process fails...
    with suppress(ValueError):
        MultiDict([BadItem()])  # type: ignore[arg-type]
