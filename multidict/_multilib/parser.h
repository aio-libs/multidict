#include "compiler.h"

#ifndef _MULTIDICT_PARSER_H
#define _MULTIDICT_PARSER_H

#ifdef __cplusplus
extern "C" {
#endif

static inline int
raise_unexpected_kwarg(const char* fname, PyObject* argname)
{
    PyErr_Format(PyExc_TypeError,
                 "%.150s() got an unexpected keyword argument '%.150U'",
                 fname,
                 argname);
    return -1;
}

static inline int
raise_multiple_values(const char* fname, PyObject* argname)
{
    PyErr_Format(PyExc_TypeError,
                 "%.150s() got multiple values for argument '%.150U'",
                 fname,
                 argname);
    return -1;
}

static inline int
raise_missing_posarg(const char* fname, PyObject* argname)
{
    PyErr_Format(PyExc_TypeError,
                 "%.150s() missing 1 required positional argument: '%.150U'",
                 fname,
                 argname);
    return -1;
}

/* The parameter names come from the module state, where they are interned,
   and so are the keyword names the compiler puts in kwnames.  Identity
   therefore settles it for a call written as f(key=...); the comparison is
   still needed for names that never went through the intern table, such as
   the ones f(**{"key": ...}) builds.  This is what CPython's own
   find_keyword() in Python/getargs.c does. */
ALWAYS_INLINE static inline int
name_is(PyObject* argname, PyObject* name)
{
    return argname == name || PyUnicode_Compare(argname, name) == 0;
}

/* Parse FASTCALL|METH_KEYWORDS arguments as two args,
the first arg is mandatory and the second one is optional
unless minargs is 2.
If the second arg is not passed it remains NULL pointer.

Both args can be passed positionally or by keyword, in any combination.

Errors are reported in the same order as CPython reports them for an
equivalent def: the keywords are walked left to right and the first
offending one wins, then the positional count, then missing args.
*/

static COLD int
parse2_slow(const char* fname, PyObject* const* args, Py_ssize_t nargs,
            PyObject* kwnames, Py_ssize_t minargs, PyObject* arg1name,
            PyObject** arg1, PyObject* arg2name, PyObject** arg2)
{
    assert(minargs >= 1);
    assert(minargs <= 2);

    *arg1 = nargs >= 1 ? args[0] : NULL;
    *arg2 = nargs >= 2 ? args[1] : NULL;

    if (kwnames != NULL) {
        // The vectorcall protocol guarantees a tuple of strings here.
        Py_ssize_t kwsize = PyTuple_GET_SIZE(kwnames);
        for (Py_ssize_t i = 0; i < kwsize; i++) {
            PyObject* argname = PyTuple_GET_ITEM(kwnames, i);  // borrowed ref
            /* The two names are distinct, so the comparison order is free.
               Try the one still unbound first: that keeps the common
               f(key, default=...) and f(key=...) forms at one comparison. */
            if (*arg1 == NULL && name_is(argname, arg1name)) {
                *arg1 = args[nargs + i];
                continue;
            }
            if (*arg2 == NULL && name_is(argname, arg2name)) {
                *arg2 = args[nargs + i];
                continue;
            }
            // Names a parameter that is already bound, or none of them.
            if (name_is(argname, arg1name)) {
                return raise_multiple_values(fname, arg1name);
            }
            if (name_is(argname, arg2name)) {
                return raise_multiple_values(fname, arg2name);
            }
            return raise_unexpected_kwarg(fname, argname);
        }
    }

    if (nargs > 2) {
        const char* txt;
        if (minargs == 2) {
            txt = "exactly 2 positional arguments";
        } else {
            txt = "from 1 to 2 positional arguments";
        }
        PyErr_Format(PyExc_TypeError,
                     "%.150s() takes %s but %zd were given",
                     fname,
                     txt,
                     nargs);
        return -1;
    }
    if (*arg1 == NULL) {
        if (minargs == 2 && *arg2 == NULL) {
            PyErr_Format(PyExc_TypeError,
                         "%.150s() missing 2 required positional arguments: "
                         "'%.150U' and '%.150U'",
                         fname,
                         arg1name,
                         arg2name);
            return -1;
        }
        return raise_missing_posarg(fname, arg1name);
    }
    if (minargs == 2 && *arg2 == NULL) {
        return raise_missing_posarg(fname, arg2name);
    }
    return 0;
}

static inline int
parse2(const char* fname, PyObject* const* args, Py_ssize_t nargs,
       PyObject* kwnames, Py_ssize_t minargs, PyObject* arg1name,
       PyObject** arg1, PyObject* arg2name, PyObject** arg2)
{
    /* Everything passed positionally and the right number of them: bind
       without touching the keyword machinery.  minargs >= 1, so nargs >=
       minargs means args[0] exists. */
    if (kwnames == NULL && nargs >= minargs && nargs <= 2) {
        *arg1 = args[0];
        *arg2 = nargs == 2 ? args[1] : NULL;
        return 0;
    }
    return parse2_slow(
        fname, args, nargs, kwnames, minargs, arg1name, arg1, arg2name, arg2);
}

#ifdef __cplusplus
}
#endif
#endif
