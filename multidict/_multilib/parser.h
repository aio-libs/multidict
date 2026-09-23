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
raise_multiple_values(const char* fname, const char* argname)
{
    PyErr_Format(PyExc_TypeError,
                 "%.150s() got multiple values for argument '%.150s'",
                 fname,
                 argname);
    return -1;
}

static inline int
raise_missing_posarg(const char* fname, const char* argname)
{
    PyErr_Format(PyExc_TypeError,
                 "%.150s() missing 1 required positional argument: '%.150s'",
                 fname,
                 argname);
    return -1;
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

static inline int
parse2(const char* fname, PyObject* const* args, Py_ssize_t nargs,
       PyObject* kwnames, Py_ssize_t minargs, const char* arg1name,
       PyObject** arg1, const char* arg2name, PyObject** arg2)
{
    assert(minargs >= 1);
    assert(minargs <= 2);

    *arg1 = nargs >= 1 ? args[0] : NULL;
    *arg2 = nargs >= 2 ? args[1] : NULL;

    if (kwnames != NULL) {
        Py_ssize_t kwsize = PyTuple_Size(kwnames);
        if (kwsize < 0) {
            return -1;
        }
        for (Py_ssize_t i = 0; i < kwsize; i++) {
            PyObject* argname = PyTuple_GetItem(kwnames, i);  // borrowed ref
            if (argname == NULL) {
                return -1;
            }
            /* The two names are distinct, so the comparison order is free.
               Try the one still unbound first: that keeps the common
               f(key, default=...) and f(key=...) forms at one comparison. */
            if (*arg1 == NULL &&
                PyUnicode_CompareWithASCIIString(argname, arg1name) == 0) {
                *arg1 = args[nargs + i];
                continue;
            }
            if (*arg2 == NULL &&
                PyUnicode_CompareWithASCIIString(argname, arg2name) == 0) {
                *arg2 = args[nargs + i];
                continue;
            }
            // Names a parameter that is already bound, or none of them.
            if (PyUnicode_CompareWithASCIIString(argname, arg1name) == 0) {
                return raise_multiple_values(fname, arg1name);
            }
            if (PyUnicode_CompareWithASCIIString(argname, arg2name) == 0) {
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
                         "'%.150s' and '%.150s'",
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

#ifdef __cplusplus
}
#endif
#endif
