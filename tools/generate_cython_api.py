"""
Generates A Multidict Cython API from multidict_api.h
by parsing the functions to generate the Cython API
Automatically.
"""

from __future__ import annotations

import argparse
import os
from dataclasses import dataclass, field
from datetime import datetime, timezone
from functools import cached_property
from pathlib import Path
from typing import Any, Callable, Sequence

from clang.cindex import Cursor, CursorKind, Index, TranslationUnit

# This uses cython by default because it's assumde you have cython installed
# if not throw an issue about it, will program a workaround

from Cython.CodeWriter import LinesResult
from Cython.Tempita import Template

__author__ = "Vizonex"
__license__ = "MIT"

# Inspired by rust's Bindgen tool they use for making C bindings
# Originally I was going to make my own new version of pxdgen that
# retained argument names but I guess it had a different use...


class CursorVisitor:
    """Inspired by Cython's Visitor Approch made for use with Clang"""

    dispatch_table: dict[CursorKind, Callable[[Cursor], None | Any]]

    def __init__(self) -> None:
        self.dispatch_table = {}

    def visit(self, cursor: Cursor) -> Any | None:
        return self._visit(cursor)

    def _visit(self, cursor: Cursor) -> Any | None:
        try:
            handler_method = self.dispatch_table[cursor.kind]
        except KeyError:
            handler_method = getattr(self, "visit_" + cursor.kind.name.lower(), None)  # type: ignore[assignment]
            self.dispatch_table[cursor.kind] = handler_method

        if handler_method is not None:
            return handler_method(cursor)
        raise RuntimeError(
            "Visitor %r does not accept %s" % (self, cursor.kind.name.lower())
        )

    def visitchildren(
        self,
        parent: Cursor,
        attrs: Sequence[CursorKind] | None = None,
        exclude: Sequence[CursorKind] | None = None,
    ) -> list[Any]:
        return self._visitchildren(parent, attrs, exclude)

    def _visitchildren(
        self,
        parent: Cursor,
        attrs: Sequence[CursorKind] | None,
        exclude: Sequence[CursorKind] | None,
    ) -> list[Any]:
        items = []
        for child in parent.get_children():
            if attrs is not None and child.kind not in attrs:
                continue
            if exclude is not None and child.kind in exclude:
                continue
            items.append(self.visit(child))
        return items


class RootVisitor(CursorVisitor):
    """Basic BaseType for working with different clang visitors"""

    def __init__(self, translation_unit: TranslationUnit, header: str):
        super().__init__()
        self.tu = translation_unit
        self.header = header
        self.data: dict[Any, Any] = {}

    def start(self) -> list[Any]:
        return [r for r in self.visitchildren(self.tu.cursor) if r is not None]

    def visit(self, cursor: Cursor) -> Any | None:
        # Filter anything this isn't us...
        if Path(str(cursor.location.file)).parts[-1] == self.header:
            return super().visit(cursor)
        return None


# === C Datatypes ===


@dataclass
class EnumType:
    name: str
    fields: list[tuple[str, int]]


@dataclass
class TypeDef:  # type: ignore[misc]
    name: str
    obj: list[Any]


@dataclass
class StructType:  # type: ignore[misc]
    name: str
    fields: list[Any]


@dataclass
class FieldType:
    name: str
    type_name: str

    @property
    def real_typename(self) -> str:
        """Determines and translates typenames"""
        if self.type_name == "PyObject *":
            return "object"
        return self.type_name


@dataclass
class PrototypeFunction:
    name: str
    fields: list[FieldType]
    ret_name: str

    def define(self) -> str:
        code = f"{self.ret_name} (*{self.name})("
        code += ", ".join([f"{f.real_typename} {f.name}" for f in self.fields]) + ")"
        return code


@dataclass
class TypeRef:
    name: str


@dataclass
class FunctionType(PrototypeFunction):
    @cached_property
    def exception_check(self) -> str:
        if "*" in self.ret_name:
            return " except NULL"
        if "int" == self.ret_name:
            return " except -1"
        return ""

    @cached_property
    def cy_return_name(self) -> str:
        """defines the Cythonic return name of a variable"""
        if self.name.endswith("New"):
            name = self.name[: self.name.find("_")]
            if name != "MultiDictIter":
                return name
            else:
                return "object"
        elif self.name.endswith("PopItem"):
            return "tuple"

        elif self.name.endswith("GetType"):
            return "type"

        elif self.name.endswith(("_CheckExact", "_Check")):
            return "int"

        elif self.name.startswith("IStr"):
            return "istr"
        else:
            return self.ret_name

    def define(self) -> str:
        # we have to underscore the definitions in order to ensure we can
        # still define the others the way we wish to define them...
        # I'll give it a C at the beginning for users who want to
        # use the CAPI directly instead of with the global __cython_multidict_api block.
        code = f'{self.ret_name} C_{self.name} "{self.name}"('
        code += ", ".join([f"{f.real_typename} {f.name}" for f in self.fields]) + ")"
        return code + self.exception_check

    def cython_definition(self, w: CythonBodyWriter) -> None:
        w.startline(f"cdef inline {self.cy_return_name} {self.name}(")
        w.endline(
            ", ".join([f"{f.real_typename} {f.name}" for f in self.fields][1:]) + "):"
        )
        w.indent()
        w.startline("return ")
        if self.ret_name != self.cy_return_name:
            w.put(f"<{self.cy_return_name}>")

        w.put(f"C_{self.name}(")

        if self.fields and self.fields[0].name == "api":
            w.put("__cython_multidict_api")
            if len(self.fields) > 1:
                w.put(", ")
            w.put(", ".join([f.name for f in self.fields][1:]))

        else:
            w.put(", ".join([f.name for f in self.fields]))

        w.endline(")")
        w.dedent()
        # Give a newline
        w.putline("")


class CythonBodyWriter:
    """Inspired by Cython's DeclarationWriter"""

    indent_string = "    "

    def __init__(self, result: LinesResult | None = None) -> None:
        super().__init__()
        if result is None:
            result = LinesResult()  # type: ignore[no-untyped-call]
        self.result = result
        self.numindents = 0

    def indent(self) -> None:
        self.numindents += 1

    def dedent(self) -> None:
        self.numindents -= 1

    def startline(self, s: str = "") -> None:
        self.result.put(self.indent_string * self.numindents + s)  # type: ignore[no-untyped-call]

    def put(self, s: str) -> None:
        self.result.put(s)  # type: ignore[no-untyped-call]

    def putline(self, s: str) -> None:
        self.result.putline(self.indent_string * self.numindents + s)  # type: ignore[no-untyped-call]

    def endline(self, s: str = "") -> None:
        self.result.putline(s)  # type: ignore[no-untyped-call]

    def line(self, s: str) -> None:
        self.startline(s)
        self.endline()

    def finish(self) -> str:
        return "\n".join(self.result.lines)


class Library:
    def __init__(self, attrs: list[Any]) -> None:
        self.attrs = attrs

    @cached_property
    def capi(self) -> StructType:
        for s in self.attrs:
            if isinstance(s, StructType):
                if s.name == "MultiDict_CAPI":
                    return s
        raise RuntimeError("MultiDict_CAPI Not found")

    def write_cython_body(self) -> str:
        w = CythonBodyWriter()
        # indent once becuase were inside of an extern
        w.indent()
        for t in self.attrs:
            w.putline("")
            if isinstance(t, EnumType):
                # === Enums ===
                w.putline(f"enum {t.name}:")
                w.indent()
                for f in t.fields:
                    w.putline(f"{f[0]} = {f[1]}")
                w.dedent()

            elif isinstance(t, StructType):
                # === Structs ===
                w.putline(f"struct {t.name}:")
                w.indent()
                for f in t.fields:
                    if isinstance(f, PrototypeFunction):
                        w.putline(f.define())  # type: ignore[unreachable]

                w.dedent()

            elif isinstance(t, TypeDef):
                # === TypeDefines ===
                # Cython compiler hates same-named things so ignore these...
                if t.name != t.obj[0].name:
                    w.putline(f"ctypedef {t.obj[0].name} {t.name}")

            elif isinstance(t, FunctionType):
                # === Functions ===
                w.putline(t.define())

        # Finally the Cherry On top...
        w.putline("MultiDict_CAPI* __cython_multidict_api")
        w.dedent()
        w.putline("")
        w.putline("# === Cython API ===")

        for t in self.attrs:
            # Filter Non-Function-Types Were only intrested in the C-API Hooks
            if not isinstance(t, FunctionType):
                continue
            t.cython_definition(w)

        return w.finish()


class CAPI_Visitor(RootVisitor):
    def start(self) -> Library:  # type: ignore[override]
        return Library(list(super().start()))

    def visit_enum_constant_decl(self, cursor: Cursor) -> tuple[str, int]:
        return (str(cursor.spelling), cursor.enum_value)

    def visit_enum_decl(self, cursor: Cursor) -> EnumType:
        return EnumType(cursor.spelling, self.visitchildren(cursor))  # type: ignore[arg-type]

    def visit_typedef_decl(self, cursor: Cursor) -> TypeDef:
        return TypeDef(cursor.spelling, self.visitchildren(cursor))  # type: ignore[arg-type]

    def visit_struct_decl(self, cursor: Cursor) -> StructType:
        children = self.visitchildren(cursor)
        return StructType(cursor.spelling, children)  # type: ignore[arg-type]

    def visit_type_ref(self, cursor: Cursor) -> TypeRef:
        return TypeRef(cursor.spelling)  # type: ignore[arg-type]

    def visit_parm_decl(self, cursor: Cursor) -> FieldType:
        return FieldType(cursor.spelling, cursor.type.spelling)  # type: ignore[arg-type]

    def visit_field_decl(self, cursor: Cursor) -> PrototypeFunction | FieldType:
        if "(*)" in cursor.type.spelling:  # type: ignore[attr-defined]
            # Prototype Function
            rettype = cursor.type.spelling.split("(*)", 1)[0].strip()
            # Not done yet give me the parameter names, We can do better than do far better than pxdgen...
            return PrototypeFunction(
                cursor.spelling,  # type:ignore[arg-type]
                self.visitchildren(cursor, exclude=[CursorKind.TYPE_REF]),
                rettype,
            )
        else:
            return FieldType(str(cursor.spelling), str(cursor.type.spelling))

    def visit_function_decl(self, cursor: Cursor) -> FunctionType:
        return FunctionType(
            str(cursor.spelling),
            self.visitchildren(cursor, attrs=[CursorKind.PARM_DECL]),
            ret_name=str(cursor.result_type.spelling),
        )

    def visit_unexposed_decl(self, cursor: Cursor) -> None:
        # Unknown so pass, visitor will filter this as something to ignore
        pass


@dataclass
class MiniBindgen:
    clang_args: list[str] = field(default_factory=list)

    def clang_arg(self, arg: str | Sequence[str]) -> None:
        """Adds in a single clang argument a list of arguments that should be joined"""
        if not isinstance(arg, str):
            arg = "".join(arg)
        self.clang_args.append(arg)

    def include(self, path: str | Path) -> None:
        r"""Used for defining a path to your header files
        this will include clang arguments under the hood
        or you. This will also transform windows paths to forward
        slashes on the fly.
        ::
            bindings = MiniBindgen()
            bindings.include("path/to/header/files")
        pathlib is also supported if multiple operating systems need supporting
        ::
            from pathlib import Path
            bindings = MiniBindgen()
            bindings.header("header.h")
            bindings.include(Path("path") / "to" / "header" / "files")
        """
        # Translate Windows Paths to forward slashes if possible.
        return self.clang_arg(("-I", Path(path).as_posix()))

    def includes(self, paths: list[str | Path]) -> None:
        """
        Same idea as `include` but it made for iterators,
        sequence types and lists \n
        See: `include` function for details
        ::
            bindings = MiniBindings()
            # to demonstrate how it can be used...
            bindings.includes(["path1", "path2", Path("custom") / "path3"])
        """
        for p in paths:
            self.include(p)

    def generate(  # type:ignore[no-untyped-def]
        self,
        header: Path | str,
        excludeDecls=False,
        options: int = 0,
        unsaved_files: list[tuple[str, str]] = [],
    ) -> CAPI_Visitor:
        """Generates a clang Translation unit to utilize"""
        index = Index.create(excludeDecls=excludeDecls)
        return CAPI_Visitor(
            index.parse(
                Path(header).as_posix(),
                self.clang_args,
                unsaved_files=unsaved_files,
                options=options,
            ),
            Path(header).parts[-1],
        )


CYTHON_API_HEAD = Template(
    """# cython: language_level = 3, freethreading_compatible=True
from cpython.object cimport PyObject, PyTypeObject
from libc.stdint cimport uint64_t

# WARNING: THIS FILE IS AUTOGENERATED DO NOT EDIT!!! 
# YOUR CHANGES WILL GET OVERWRITTEN AND YOUR POOR EDITS WILL BE GONE!!!
# GENERATED ON: {{date}}
# COMPILIED BY: {{author}}

cdef extern from "multidict_api.h":
    \"\"\"
/* Extra Data comes from multidict.__init__.pxd */
/* Ensure we can obtain the Functions we wish to utilize */
#define MULTIDICT_IMPL
MultiDict_CAPI* __cython_multidict_api;
// Redefinitions incase required by cython...
// Don't want size calculations to get screwed up...
#if PY_VERSION_HEX >= 0x030c00f0
#define __MANAGED_WEAKREFS
#endif
typedef struct {
    PyObject_HEAD
#ifndef __MANAGED_WEAKREFS
    PyObject *weaklist;
#endif
    // we can ignore state however we already know it's 
    // a size of 4/8 depending on 32/64 bit...
    void *state; 
    Py_ssize_t used;
    uint64_t version;
    bool is_ci;
    htkeys_t *keys;
} MultiDictObject;
typedef struct {
    PyObject_HEAD
#ifndef __MANAGED_WEAKREFS
    PyObject *weaklist;
#endif
    MultiDictObject *md;
} MultiDictProxyObject;
typedef struct {
    PyUnicodeObject str;
    PyObject *canonical;
    void *state;
} istrobject;
int multidict_import(){
    __cython_multidict_api = MultiDict_Import();
    return __cython_multidict_api != NULL;
}
    \"\"\"
    # NOTE: Important that you import this 
    # After you've c-imported multidict 
    int multidict_import() except 0
    # Predefined objects from istr & multidict
    ctypedef struct MultiDictObject:
        pass
    
    ctypedef struct MultiDictProxyObject:
        pass
    
    ctypedef struct istrobject:
        pass 
        
    ctypedef class _multidict.istr [object istrobject, check_size ignore]:
        pass
    
    ctypedef class _multidict.MultiDict [object MultiDictObject, check_size ignore]:
        pass
    
    ctypedef class _multidict.CIMultiDict [object MultiDictObject, check_size ignore]:
        pass
    
    ctypedef class _multidict.MultiDictProxy [object MultiDictProxyObject, check_size ignore]:
        pass
    
    ctypedef class _multidict.CIMultiDictProxy [object MultiDictProxyObject, check_size ignore]:
        pass
    
    
"""
)  # type: ignore[no-untyped-call]


def scan_header_file(header_file: str, author_name: str = "anonymous") -> str:
    bg = MiniBindgen()
    # clang needs to be able to identify PyObjects and PyTypeObjects
    # this should be able to find a user's Python Path to where Python.h
    # is stored. If this is not you I am in the middle of making a workaround for that...
    bg.include(Path(os.environ["PYTHONPATH"]) / "include")
    # print(bg)
    data: Library = bg.generate(header_file).start()

    file: str = CYTHON_API_HEAD.substitute(
        author=author_name, date=datetime.now(timezone.utc)
    )  # type: ignore[no-untyped-call]

    file += data.write_cython_body()
    return file


def cli() -> None:
    parser = argparse.ArgumentParser(
        description="Compiles Multidict Cython pxd file using clang"
    )
    parser.add_argument(
        "-a", "--author", help="who's compiling the code", default="anonymous"
    )
    parser.add_argument("-o", "--output", default="multidict/__init__.pxd")
    parser.add_argument(
        "-i", "--input", help="input header file", default="multidict/multidict_api.h"
    )
    args = parser.parse_args()
    data = scan_header_file(args.input, args.author)
    with open(args.output, "w") as w:
        w.write(data)


if __name__ == "__main__":
    cli()
