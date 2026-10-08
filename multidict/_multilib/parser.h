#include "compiler.h"

#ifndef _MULTIDICT_PARSER_H
#define _MULTIDICT_PARSER_H

#ifdef __cplusplus
extern "C" {
#endif

COLD static void
_raise_unexpected_kwarg(const char* fname, PyObject* argname)
{
    PyErr_Format(PyExc_TypeError,
                 "%.150s() got an unexpected keyword argument '%.150U'",
                 fname,
                 argname);
}

COLD static void
_raise_multiple_values(const char* fname, PyObject* argname)
{
    PyErr_Format(PyExc_TypeError,
                 "%.150s() got multiple values for argument '%.150U'",
                 fname,
                 argname);
}

COLD static void
_raise_missing_posarg(const char* fname, PyObject* argname)
{
    PyErr_Format(PyExc_TypeError,
                 "%.150s() missing 1 required positional argument: '%.150U'",
                 fname,
                 argname);
}

/* The parameter names come from the module state, where they are interned,
   and so are the keyword names the compiler puts in kwnames.  Identity
   therefore settles it for a call written as f(key=...); the comparison is
   still needed for names that never went through the intern table, such as
   the ones f(**{"key": ...}) builds.  This is what CPython's own
   find_keyword() in Python/getargs.c does. */
static int
_name_is(PyObject* argname, PyObject* name)
{
    return argname == name || PyUnicode_Compare(argname, name) == 0;
}

/* Returned by value, not through out-parameters: those would make every
   caller's locals address-taken, and -fstack-protector-strong then gives each
   call a canary for a path only keyword calls take.  arg1 is NULL on error;
   a successful bind always sets it, since minargs >= 1. */
typedef struct {
    PyObject* arg1;
    PyObject* arg2;
} parse2_t;

/* Parse FASTCALL|METH_KEYWORDS arguments as two args,
the first arg is mandatory and the second one is optional
unless minargs is 2.
If the second arg is not passed it remains NULL pointer.

Both args can be passed positionally or by keyword, in any combination.

Errors are reported in the same order as CPython reports them for an
equivalent def: the keywords are walked left to right and the first
offending one wins, then the positional count, then missing args.
*/

static COLD parse2_t
_parse2_slow(const char* fname, PyObject* const* args, Py_ssize_t nargs,
             PyObject* kwnames, Py_ssize_t minargs, PyObject* arg1name,
             PyObject* arg2name)
{
    parse2_t r;
    assert(minargs >= 1);
    assert(minargs <= 2);

    r.arg1 = nargs >= 1 ? args[0] : NULL;
    r.arg2 = nargs >= 2 ? args[1] : NULL;

    if (kwnames != NULL) {
        // The vectorcall protocol guarantees a tuple of strings here.
        Py_ssize_t kwsize = PyTuple_GET_SIZE(kwnames);
        for (Py_ssize_t i = 0; i < kwsize; i++) {
            PyObject* argname = PyTuple_GET_ITEM(kwnames, i);  // borrowed ref
            /* The two names are distinct, so the comparison order is free.
               Try the one still unbound first: that keeps the common
               f(key, default=...) and f(key=...) forms at one comparison. */
            if (r.arg1 == NULL && _name_is(argname, arg1name)) {
                r.arg1 = args[nargs + i];
                continue;
            }
            if (r.arg2 == NULL && _name_is(argname, arg2name)) {
                r.arg2 = args[nargs + i];
                continue;
            }
            // Names a parameter that is already bound, or none of them.
            if (_name_is(argname, arg1name)) {
                _raise_multiple_values(fname, arg1name);
                goto fail;
            }
            if (_name_is(argname, arg2name)) {
                _raise_multiple_values(fname, arg2name);
                goto fail;
            }
            _raise_unexpected_kwarg(fname, argname);
            goto fail;
        }
    }

    if (UNLIKELY(nargs > 2)) {
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
        goto fail;
    }
    if (UNLIKELY(r.arg1 == NULL)) {
        if (minargs == 2 && r.arg2 == NULL) {
            PyErr_Format(PyExc_TypeError,
                         "%.150s() missing 2 required positional arguments: "
                         "'%.150U' and '%.150U'",
                         fname,
                         arg1name,
                         arg2name);
            goto fail;
        }
        _raise_missing_posarg(fname, arg1name);
        goto fail;
    }
    if (UNLIKELY(minargs == 2 && r.arg2 == NULL)) {
        _raise_missing_posarg(fname, arg2name);
        goto fail;
    }
    return r;
fail:
    r.arg1 = NULL;
    return r;
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
    parse2_t r =
        _parse2_slow(fname, args, nargs, kwnames, minargs, arg1name, arg2name);
    if (r.arg1 == NULL) {
        return -1;
    }
    *arg1 = r.arg1;
    *arg2 = r.arg2;
    return 0;
}

#ifdef __cplusplus
}
#endif
#endif
