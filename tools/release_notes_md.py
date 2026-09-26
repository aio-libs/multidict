"""Rewrite the Sphinx roles in CHANGES.rst as Markdown for a GitHub Release.

The release workflow publishes a section of CHANGES.rst verbatim as the
GitHub Release body, which GitHub renders as Markdown. Headings, lists,
emphasis and ``literals`` already read correctly there; roles do not.
"""

import re
import sys

GITHUB = "https://github.com"
REPO = f"{GITHUB}/aio-libs/multidict"
DOCS = "https://multidict.aio-libs.org/en/stable"

ROLE_RE = re.compile(r":(?:(?:py|c):)?(?P<role>[a-z]+):`(?P<body>[^`]+)`")
TITLED_RE = re.compile(r"(?P<title>.*?)\s*<(?P<target>[^<>]+)>", re.DOTALL)
CALLABLE_ROLES = frozenset({"meth", "func"})


def _convert(match: re.Match[str]) -> str:
    role = match["role"]
    body = " ".join(match["body"].split())
    title = None
    if titled := TITLED_RE.fullmatch(body):
        title, body = titled["title"], titled["target"]

    if role in ("issue", "pr"):
        return f"#{body}"
    if role == "commit":
        return f"[{body}]({REPO}/commit/{body})"
    if role == "user":
        return f"[@{body}]({GITHUB}/{body})"
    if role == "gh":
        return f"[{title or body}]({GITHUB}/{body})"
    if role == "doc":
        return f"[{title or body}]({DOCS}/{body}.html)"
    if title is not None:
        return title

    body = body.removeprefix("!")
    if body.startswith("~"):
        body = body.rpartition(".")[2]
    if role in CALLABLE_ROLES:
        body += "()"
    return f"`{body}`"


def convert(text: str) -> str:
    return ROLE_RE.sub(_convert, text)


def main() -> None:
    src, dst = sys.argv[1:]
    with open(src, encoding="utf-8") as f:
        text = f.read()
    with open(dst, "w", encoding="utf-8") as f:
        f.write(convert(text))


if __name__ == "__main__":
    main()
