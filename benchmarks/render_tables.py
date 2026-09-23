"""Turn ``callgrind_driver.py`` output into the tables in ``docs/benchmark.rst``.

    python benchmarks/render_tables.py gil.json ft.json
    python benchmarks/render_tables.py gil.json ft.json --write docs/benchmark.rst

Without ``--write`` the reStructuredText goes to stdout, so the numbers can be
read before they are committed.  With it, the span between the two marker
comments in the target file is replaced.
"""

import argparse
import json
import statistics
import sys

BEGIN = ".. BEGIN GENERATED TABLES"
END = ".. END GENERATED TABLES"

C_COLUMNS = ("dict", "multidict_c", "cimultidict_c")
PY_COLUMNS = ("multidict_py", "cimultidict_py")


def load(path: str) -> dict:
    with open(path) as fp:
        return json.load(fp)


def list_table(
    caption: str, widths: list[int], header: list[str], rows: list[list[str]]
) -> str:
    out = [
        f".. list-table:: {caption}",
        "   :header-rows: 1",
        f"   :widths: {' '.join(str(w) for w in widths)}",
        "",
    ]
    for row in [header, *rows]:
        out.append(f"   * - {row[0]}")
        out.extend(f"     - {cell}" for cell in row[1:])
    out.append("")
    return "\n".join(out)


def ir(data: dict, op_id: str, impl_id: str) -> float | None:
    cell = data["cells"].get(op_id, {}).get(impl_id)
    return cell["ir_per_op"] if cell else None


def fmt(value: float | None) -> str:
    return "--" if value is None else f"{value:,.0f}"


def ratio(value: float | None, base: float | None) -> str:
    if value is None or not base:
        return "--"
    return f"{value / base:.2f}x"


def build_label(data: dict) -> str:
    version = data["metadata"]["python_version"]
    build = "free-threaded" if data["metadata"]["gil_disabled"] else "GIL"
    return f"``CPython {version}``, {build} build"


def c_table(data: dict, op_ids: list[str]) -> str:
    rows = []
    for op_id in op_ids:
        base = ir(data, op_id, "dict")
        cells = [ir(data, op_id, impl_id) for impl_id in C_COLUMNS]
        rows.append(
            [data["operations"][op_id], *(fmt(v) for v in cells), ratio(cells[2], base)]
        )
    return list_table(
        f"Instructions per operation, {build_label(data)}",
        [32, 14, 16, 18, 20],
        [
            "Operation",
            "``dict``",
            "``MultiDict``",
            "``CIMultiDict``",
            "``CIMultiDict`` vs ``dict``",
        ],
        rows,
    )


def py_table(data: dict, op_ids: list[str]) -> str:
    rows = []
    for op_id in op_ids:
        py_md = ir(data, op_id, "multidict_py")
        py_ci = ir(data, op_id, "cimultidict_py")
        c_md = ir(data, op_id, "multidict_c")
        rows.append(
            [data["operations"][op_id], fmt(py_md), fmt(py_ci), ratio(py_md, c_md)]
        )
    return list_table(
        f"Pure-Python backend, instructions per operation, {build_label(data)}",
        [34, 20, 20, 26],
        [
            "Operation",
            "``MultiDict``",
            "``CIMultiDict``",
            "``MultiDict`` vs the C extension",
        ],
        rows,
    )


def overhead_table(gil: dict, ft: dict, op_ids: list[str]) -> str:
    rows = []
    for impl_id in (*C_COLUMNS, *PY_COLUMNS):
        pairs = []
        for op_id in op_ids:
            before, after = ir(gil, op_id, impl_id), ir(ft, op_id, impl_id)
            if before and after:
                pairs.append((after / before, op_id))
        if not pairs:
            continue
        best, worst = min(pairs), max(pairs)
        rows.append(
            [
                gil["implementations"][impl_id],
                f"{statistics.median(r for r, _ in pairs):.2f}x",
                f"{best[0]:.2f}x",
                f"{worst[0]:.2f}x",
                gil["operations"][worst[1]],
            ]
        )
    caption = (
        f"Free-threading overhead, ``{ft['metadata']['python_version']}`` "
        "free-threaded versus GIL build"
    )
    return list_table(
        caption,
        [26, 15, 12, 13, 24],
        ["Class", "Median", "Best", "Worst", "Worst operation"],
        rows,
    )


def provenance(gil: dict, ft: dict) -> str:
    meta, ft_meta = gil["metadata"], ft["metadata"]
    counting = "client-request bracketing" if meta["bracketed"] else "whole process"
    sha = meta["git_sha"] + (", dirty" if meta["git_dirty"] else "")
    version = meta["valgrind_version"].split()[-1].removeprefix("valgrind-")
    lines = [
        ".. code-block:: text",
        "",
        f"   multidict   {meta['multidict_version']} ({sha})",
        f"   CPython     {meta['python_version']}, GIL and free-threaded builds",
        f"   valgrind    {version}, callgrind, {counting}",
        f"   CPU         {meta['cpu_model']}",
        f"   platform    {meta['platform']}",
        f"   collected   {meta['created'][:10]}",
        "",
    ]
    return "\n".join(lines)


def render(gil: dict, ft: dict) -> str:
    op_ids = [op_id for op_id in gil["operations"] if op_id in ft["operations"]]
    return "\n".join(
        [
            provenance(gil, ft),
            c_table(gil, op_ids),
            c_table(ft, op_ids),
            overhead_table(gil, ft, op_ids),
            py_table(gil, op_ids),
        ]
    )


def check_comparable(gil: dict, ft: dict, allow_skew: bool) -> None:
    if gil["metadata"]["gil_disabled"] or not ft["metadata"]["gil_disabled"]:
        raise SystemExit(
            "error: pass the GIL build's JSON first and the free-threaded one second"
        )
    for key in ("python_version", "multidict_version", "git_sha"):
        if gil["metadata"][key] != ft["metadata"][key]:
            message = (
                f"error: the two runs disagree on {key} "
                f"({gil['metadata'][key]} vs {ft['metadata'][key]}); the "
                "free-threading table would attribute that difference to "
                "free-threading. Pass --allow-version-skew to render anyway."
            )
            if not allow_skew:
                raise SystemExit(message)
            print(message.replace("error:", "warning:"), file=sys.stderr)


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("gil_json", help="driver output from the GIL build")
    parser.add_argument("ft_json", help="driver output from the free-threaded build")
    parser.add_argument("--write", metavar="RST", help="update this file in place")
    parser.add_argument("--allow-version-skew", action="store_true")
    args = parser.parse_args()

    gil, ft = load(args.gil_json), load(args.ft_json)
    check_comparable(gil, ft, args.allow_version_skew)
    tables = render(gil, ft)

    if not args.write:
        print(tables)
        return 0

    unbracketed = [
        path
        for path, data in ((args.gil_json, gil), (args.ft_json, ft))
        if not data["metadata"]["bracketed"]
    ]
    if unbracketed:
        raise SystemExit(
            f"error: {', '.join(unbracketed)} counted the whole process, which "
            "undercounts every operation that leaves the mapping smaller than "
            "the baseline arm does. Install pytest-codspeed so the driver can "
            "bracket the measured region, and collect again."
        )

    with open(args.write) as fp:
        text = fp.read()
    try:
        head, rest = text.split(BEGIN, 1)
        _, tail = rest.split(END, 1)
    except ValueError:
        raise SystemExit(
            f"error: {args.write} has no {BEGIN} / {END} markers"
        ) from None
    with open(args.write, "w") as fp:
        fp.write(f"{head}{BEGIN}\n\n{tables}\n{END}{tail}")
    print(f"updated {args.write}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
