// Fastcall helpers

typedef struct _PyArg_Parser {
    const char *format;
    const char * const *keywords;
    const char *fname;
    const char *custom_msg;
    int pos;            /* number of positional-only arguments */
    int min;            /* minimal number of arguments */
    int max;            /* maximal number of positional arguments */
    PyObject *kwtuple;  /* tuple of keyword parameter names */
    struct _PyArg_Parser *next;
} _PyArg_Parser;

/* List of static parsers. */
static struct _PyArg_Parser *static_arg_parsers = NULL;

static int
parser_init(struct _PyArg_Parser *parser)
{
    const char * const *keywords;
    const char *format, *msg;
    int i, len, min, max, nkw;
    PyObject *kwtuple;

    assert(parser->keywords != NULL);
    if (parser->kwtuple != NULL) {
        return 1;
    }

    keywords = parser->keywords;
    /* scan keywords and count the number of positional-only parameters */
    for (i = 0; keywords[i] && !*keywords[i]; i++) {
    }
    parser->pos = i;
    /* scan keywords and get greatest possible nbr of args */
    for (; keywords[i]; i++) {
        if (!*keywords[i]) {
            PyErr_SetString(PyExc_SystemError,
                            "Empty keyword parameter name");
            return 0;
        }
    }
    len = i;

    format = parser->format;
    if (format) {
        /* grab the function name or custom error msg first (mutually exclusive) */
        parser->fname = strchr(parser->format, ':');
        if (parser->fname) {
            parser->fname++;
            parser->custom_msg = NULL;
        }
        else {
            parser->custom_msg = strchr(parser->format,';');
            if (parser->custom_msg)
                parser->custom_msg++;
        }

        min = max = INT_MAX;
        for (i = 0; i < len; i++) {
            if (*format == '|') {
                if (min != INT_MAX) {
                    PyErr_SetString(PyExc_SystemError,
                                    "Invalid format string (| specified twice)");
                    return 0;
                }
                if (max != INT_MAX) {
                    PyErr_SetString(PyExc_SystemError,
                                    "Invalid format string ($ before |)");
                    return 0;
                }
                min = i;
                format++;
            }
            if (*format == '$') {
                if (max != INT_MAX) {
                    PyErr_SetString(PyExc_SystemError,
                                    "Invalid format string ($ specified twice)");
                    return 0;
                }
                if (i < parser->pos) {
                    PyErr_SetString(PyExc_SystemError,
                                    "Empty parameter name after $");
                    return 0;
                }
                max = i;
                format++;
            }
            if (IS_END_OF_FORMAT(*format)) {
                PyErr_Format(PyExc_SystemError,
                            "More keyword list entries (%d) than "
                            "format specifiers (%d)", len, i);
                return 0;
            }

            msg = skipitem(&format, NULL, 0);
            if (msg) {
                PyErr_Format(PyExc_SystemError, "%s: '%s'", msg,
                            format);
                return 0;
            }
        }
        parser->min = Py_MIN(min, len);
        parser->max = Py_MIN(max, len);

        if (!IS_END_OF_FORMAT(*format) && (*format != '|') && (*format != '$')) {
            PyErr_Format(PyExc_SystemError,
                "more argument specifiers than keyword list entries "
                "(remaining format:'%s')", format);
            return 0;
        }
    }

    nkw = len - parser->pos;
    kwtuple = PyTuple_New(nkw);
    if (kwtuple == NULL) {
        return 0;
    }
    keywords = parser->keywords + parser->pos;
    for (i = 0; i < nkw; i++) {
        PyObject *str = PyUnicode_FromString(keywords[i]);
        if (str == NULL) {
            Py_DECREF(kwtuple);
            return 0;
        }
        PyUnicode_InternInPlace(&str);
        PyTuple_SET_ITEM(kwtuple, i, str);
    }
    parser->kwtuple = kwtuple;

    assert(parser->next == NULL);
    parser->next = static_arg_parsers;
    static_arg_parsers = parser;
    return 1;
}

static void
parser_clear(struct _PyArg_Parser *parser)
{
    Py_CLEAR(parser->kwtuple);
}

static PyObject*
find_keyword(PyObject *kwnames, PyObject *const *kwstack, PyObject *key)
{
    Py_ssize_t i, nkwargs;

    nkwargs = PyTuple_GET_SIZE(kwnames);
    for (i=0; i < nkwargs; i++) {
        PyObject *kwname = PyTuple_GET_ITEM(kwnames, i);

        /* ptr==ptr should match in most cases since keyword keys
           should be interned strings */
        if (kwname == key) {
            return kwstack[i];
        }
        if (!PyUnicode_Check(kwname)) {
            /* ignore non-string keyword keys:
               an error will be raised below */
            continue;
        }
        if (_PyUnicode_EQ(kwname, key)) {
            return kwstack[i];
        }
    }
    return NULL;
}

static int
vgetargskeywordsfast_impl(PyObject *const *args, Py_ssize_t nargs,
                          PyObject *kwargs, PyObject *kwnames,
                          struct _PyArg_Parser *parser,
                          va_list *p_va, int flags)
{
    PyObject *kwtuple;
    char msgbuf[512];
    int levels[32];
    const char *format;
    const char *msg;
    PyObject *keyword;
    int i, pos, len;
    Py_ssize_t nkwargs;
    PyObject *current_arg;
    freelistentry_t static_entries[STATIC_FREELIST_ENTRIES];
    freelist_t freelist;
    PyObject *const *kwstack = NULL;

    freelist.entries = static_entries;
    freelist.first_available = 0;
    freelist.entries_malloced = 0;

    assert(kwargs == NULL || PyDict_Check(kwargs));
    assert(kwargs == NULL || kwnames == NULL);
    assert(p_va != NULL);

    if (parser == NULL) {
        PyErr_BadInternalCall();
        return 0;
    }

    if (kwnames != NULL && !PyTuple_Check(kwnames)) {
        PyErr_BadInternalCall();
        return 0;
    }

    if (!parser_init(parser)) {
        return 0;
    }

    kwtuple = parser->kwtuple;
    pos = parser->pos;
    len = pos + (int)PyTuple_GET_SIZE(kwtuple);

    if (len > STATIC_FREELIST_ENTRIES) {
        freelist.entries = PyMem_NEW(freelistentry_t, len);
        if (freelist.entries == NULL) {
            PyErr_NoMemory();
            return 0;
        }
        freelist.entries_malloced = 1;
    }

    if (kwargs != NULL) {
        nkwargs = PyDict_GET_SIZE(kwargs);
    }
    else if (kwnames != NULL) {
        nkwargs = PyTuple_GET_SIZE(kwnames);
        kwstack = args + nargs;
    }
    else {
        nkwargs = 0;
    }
    if (nargs + nkwargs > len) {
        /* Adding "keyword" (when nargs == 0) prevents producing wrong error
           messages in some special cases (see bpo-31229). */
        PyErr_Format(PyExc_TypeError,
                     "%.200s%s takes at most %d %sargument%s (%zd given)",
                     (parser->fname == NULL) ? "function" : parser->fname,
                     (parser->fname == NULL) ? "" : "()",
                     len,
                     (nargs == 0) ? "keyword " : "",
                     (len == 1) ? "" : "s",
                     nargs + nkwargs);
        return cleanreturn(0, &freelist);
    }
    if (parser->max < nargs) {
        if (parser->max == 0) {
            PyErr_Format(PyExc_TypeError,
                         "%.200s%s takes no positional arguments",
                         (parser->fname == NULL) ? "function" : parser->fname,
                         (parser->fname == NULL) ? "" : "()");
        }
        else {
            PyErr_Format(PyExc_TypeError,
                         "%.200s%s takes %s %d positional argument%s (%zd given)",
                         (parser->fname == NULL) ? "function" : parser->fname,
                         (parser->fname == NULL) ? "" : "()",
                         (parser->min < parser->max) ? "at most" : "exactly",
                         parser->max,
                         parser->max == 1 ? "" : "s",
                         nargs);
        }
        return cleanreturn(0, &freelist);
    }

    format = parser->format;
    /* convert tuple args and keyword args in same loop, using kwtuple to drive process */
    for (i = 0; i < len; i++) {
        if (*format == '|') {
            format++;
        }
        if (*format == '$') {
            format++;
        }
        assert(!IS_END_OF_FORMAT(*format));

        if (i < nargs) {
            current_arg = args[i];
        }
        else if (nkwargs && i >= pos) {
            keyword = PyTuple_GET_ITEM(kwtuple, i - pos);
            if (kwargs != NULL) {
                current_arg = PyDict_GetItemWithError(kwargs, keyword);
                if (!current_arg && PyErr_Occurred()) {
                    return cleanreturn(0, &freelist);
                }
            }
            else {
                current_arg = find_keyword(kwnames, kwstack, keyword);
            }
            if (current_arg) {
                --nkwargs;
            }
        }
        else {
            current_arg = NULL;
        }

        if (current_arg) {
            msg = convertitem(current_arg, &format, p_va, flags,
                levels, msgbuf, sizeof(msgbuf), &freelist);
            if (msg) {
                seterror(i+1, msg, levels, parser->fname, parser->custom_msg);
                return cleanreturn(0, &freelist);
            }
            continue;
        }

        if (i < parser->min) {
            /* Less arguments than required */
            if (i < pos) {
                Py_ssize_t min = Py_MIN(pos, parser->min);
                PyErr_Format(PyExc_TypeError,
                             "%.200s%s takes %s %d positional argument%s"
                             " (%zd given)",
                             (parser->fname == NULL) ? "function" : parser->fname,
                             (parser->fname == NULL) ? "" : "()",
                             min < parser->max ? "at least" : "exactly",
                             min,
                             min == 1 ? "" : "s",
                             nargs);
            }
            else {
                keyword = PyTuple_GET_ITEM(kwtuple, i - pos);
                PyErr_Format(PyExc_TypeError,  "%.200s%s missing required "
                             "argument '%U' (pos %d)",
                             (parser->fname == NULL) ? "function" : parser->fname,
                             (parser->fname == NULL) ? "" : "()",
                             keyword, i+1);
            }
            return cleanreturn(0, &freelist);
        }
        /* current code reports success when all required args
         * fulfilled and no keyword args left, with no further
         * validation. XXX Maybe skip this in debug build ?
         */
        if (!nkwargs) {
            return cleanreturn(1, &freelist);
        }

        /* We are into optional args, skip through to any remaining
         * keyword args */
        msg = skipitem(&format, p_va, flags);
        assert(msg == NULL);
    }

    assert(IS_END_OF_FORMAT(*format) || (*format == '|') || (*format == '$'));

    if (nkwargs > 0) {
        Py_ssize_t j;
        /* make sure there are no arguments given by name and position */
        for (i = pos; i < nargs; i++) {
            keyword = PyTuple_GET_ITEM(kwtuple, i - pos);
            if (kwargs != NULL) {
                current_arg = PyDict_GetItemWithError(kwargs, keyword);
                if (!current_arg && PyErr_Occurred()) {
                    return cleanreturn(0, &freelist);
                }
            }
            else {
                current_arg = find_keyword(kwnames, kwstack, keyword);
            }
            if (current_arg) {
                /* arg present in tuple and in dict */
                PyErr_Format(PyExc_TypeError,
                             "argument for %.200s%s given by name ('%U') "
                             "and position (%d)",
                             (parser->fname == NULL) ? "function" : parser->fname,
                             (parser->fname == NULL) ? "" : "()",
                             keyword, i+1);
                return cleanreturn(0, &freelist);
            }
        }
        /* make sure there are no extraneous keyword arguments */
        j = 0;
        while (1) {
            int match;
            if (kwargs != NULL) {
                if (!PyDict_Next(kwargs, &j, &keyword, NULL))
                    break;
            }
            else {
                if (j >= PyTuple_GET_SIZE(kwnames))
                    break;
                keyword = PyTuple_GET_ITEM(kwnames, j);
                j++;
            }

            if (!PyUnicode_Check(keyword)) {
                PyErr_SetString(PyExc_TypeError,
                                "keywords must be strings");
                return cleanreturn(0, &freelist);
            }
            match = PySequence_Contains(kwtuple, keyword);
            if (match <= 0) {
                if (!match) {
                    PyErr_Format(PyExc_TypeError,
                                 "'%U' is an invalid keyword "
                                 "argument for %.200s%s",
                                 keyword,
                                 (parser->fname == NULL) ? "this function" : parser->fname,
                                 (parser->fname == NULL) ? "" : "()");
                }
                return cleanreturn(0, &freelist);
            }
        }
    }

    return cleanreturn(1, &freelist);
}

static int
vgetargskeywordsfast(PyObject *args, PyObject *keywords,
                     struct _PyArg_Parser *parser, va_list *p_va, int flags)
{
    PyObject **stack;
    Py_ssize_t nargs;

    if (args == NULL
        || !PyTuple_Check(args)
        || (keywords != NULL && !PyDict_Check(keywords)))
    {
        PyErr_BadInternalCall();
        return 0;
    }

    stack = _PyTuple_ITEMS(args);
    nargs = PyTuple_GET_SIZE(args);
    return vgetargskeywordsfast_impl(stack, nargs, keywords, NULL,
                                     parser, p_va, flags);
}



static PyObject * const *
_PyArg_UnpackKeywords(PyObject *const *args, Py_ssize_t nargs,
                      PyObject *kwargs, PyObject *kwnames,
                      struct _PyArg_Parser *parser,
                      int minpos, int maxpos, int minkw,
                      PyObject **buf)
{
    PyObject *kwtuple;
    PyObject *keyword;
    int i, posonly, minposonly, maxargs;
    int reqlimit = minkw ? maxpos + minkw : minpos;
    Py_ssize_t nkwargs;
    PyObject *current_arg;
    PyObject * const *kwstack = NULL;

    assert(kwargs == NULL || PyDict_Check(kwargs));
    assert(kwargs == NULL || kwnames == NULL);

    if (parser == NULL) {
        PyErr_BadInternalCall();
        return NULL;
    }

    if (kwnames != NULL && !PyTuple_Check(kwnames)) {
        PyErr_BadInternalCall();
        return NULL;
    }

    if (args == NULL && nargs == 0) {
        args = buf;
    }

    if (!parser_init(parser)) {
        return NULL;
    }

    kwtuple = parser->kwtuple;
    posonly = parser->pos;
    minposonly = Py_MIN(posonly, minpos);
    maxargs = posonly + (int)PyTuple_GET_SIZE(kwtuple);

    if (kwargs != NULL) {
        nkwargs = PyDict_GET_SIZE(kwargs);
    }
    else if (kwnames != NULL) {
        nkwargs = PyTuple_GET_SIZE(kwnames);
        kwstack = args + nargs;
    }
    else {
        nkwargs = 0;
    }
    if (nkwargs == 0 && minkw == 0 && minpos <= nargs && nargs <= maxpos) {
        /* Fast path. */
        return args;
    }
    if (nargs + nkwargs > maxargs) {
        /* Adding "keyword" (when nargs == 0) prevents producing wrong error
           messages in some special cases (see bpo-31229). */
        PyErr_Format(PyExc_TypeError,
                     "%.200s%s takes at most %d %sargument%s (%zd given)",
                     (parser->fname == NULL) ? "function" : parser->fname,
                     (parser->fname == NULL) ? "" : "()",
                     maxargs,
                     (nargs == 0) ? "keyword " : "",
                     (maxargs == 1) ? "" : "s",
                     nargs + nkwargs);
        return NULL;
    }
    if (nargs > maxpos) {
        if (maxpos == 0) {
            PyErr_Format(PyExc_TypeError,
                         "%.200s%s takes no positional arguments",
                         (parser->fname == NULL) ? "function" : parser->fname,
                         (parser->fname == NULL) ? "" : "()");
        }
        else {
            PyErr_Format(PyExc_TypeError,
                         "%.200s%s takes %s %d positional argument%s (%zd given)",
                         (parser->fname == NULL) ? "function" : parser->fname,
                         (parser->fname == NULL) ? "" : "()",
                         (minpos < maxpos) ? "at most" : "exactly",
                         maxpos,
                         (maxpos == 1) ? "" : "s",
                         nargs);
        }
        return NULL;
    }
    if (nargs < minposonly) {
        PyErr_Format(PyExc_TypeError,
                     "%.200s%s takes %s %d positional argument%s"
                     " (%zd given)",
                     (parser->fname == NULL) ? "function" : parser->fname,
                     (parser->fname == NULL) ? "" : "()",
                     minposonly < maxpos ? "at least" : "exactly",
                     minposonly,
                     minposonly == 1 ? "" : "s",
                     nargs);
        return NULL;
    }

    /* copy tuple args */
    for (i = 0; i < nargs; i++) {
        buf[i] = args[i];
    }

    /* copy keyword args using kwtuple to drive process */
    for (i = Py_MAX((int)nargs, posonly); i < maxargs; i++) {
        if (nkwargs) {
            keyword = PyTuple_GET_ITEM(kwtuple, i - posonly);
            if (kwargs != NULL) {
                current_arg = PyDict_GetItemWithError(kwargs, keyword);
                if (!current_arg && PyErr_Occurred()) {
                    return NULL;
                }
            }
            else {
                current_arg = find_keyword(kwnames, kwstack, keyword);
            }
        }
        else if (i >= reqlimit) {
            break;
        }
        else {
            current_arg = NULL;
        }

        buf[i] = current_arg;

        if (current_arg) {
            --nkwargs;
        }
        else if (i < minpos || (maxpos <= i && i < reqlimit)) {
            /* Less arguments than required */
            keyword = PyTuple_GET_ITEM(kwtuple, i - posonly);
            PyErr_Format(PyExc_TypeError,  "%.200s%s missing required "
                         "argument '%U' (pos %d)",
                         (parser->fname == NULL) ? "function" : parser->fname,
                         (parser->fname == NULL) ? "" : "()",
                         keyword, i+1);
            return NULL;
        }
    }

    if (nkwargs > 0) {
        Py_ssize_t j;
        /* make sure there are no arguments given by name and position */
        for (i = posonly; i < nargs; i++) {
            keyword = PyTuple_GET_ITEM(kwtuple, i - posonly);
            if (kwargs != NULL) {
                current_arg = PyDict_GetItemWithError(kwargs, keyword);
                if (!current_arg && PyErr_Occurred()) {
                    return NULL;
                }
            }
            else {
                current_arg = find_keyword(kwnames, kwstack, keyword);
            }
            if (current_arg) {
                /* arg present in tuple and in dict */
                PyErr_Format(PyExc_TypeError,
                             "argument for %.200s%s given by name ('%U') "
                             "and position (%d)",
                             (parser->fname == NULL) ? "function" : parser->fname,
                             (parser->fname == NULL) ? "" : "()",
                             keyword, i+1);
                return NULL;
            }
        }
        /* make sure there are no extraneous keyword arguments */
        j = 0;
        while (1) {
            int match;
            if (kwargs != NULL) {
                if (!PyDict_Next(kwargs, &j, &keyword, NULL))
                    break;
            }
            else {
                if (j >= PyTuple_GET_SIZE(kwnames))
                    break;
                keyword = PyTuple_GET_ITEM(kwnames, j);
                j++;
            }

            if (!PyUnicode_Check(keyword)) {
                PyErr_SetString(PyExc_TypeError,
                                "keywords must be strings");
                return NULL;
            }
            match = PySequence_Contains(kwtuple, keyword);
            if (match <= 0) {
                if (!match) {
                    PyErr_Format(PyExc_TypeError,
                                 "'%U' is an invalid keyword "
                                 "argument for %.200s%s",
                                 keyword,
                                 (parser->fname == NULL) ? "this function" : parser->fname,
                                 (parser->fname == NULL) ? "" : "()");
                }
                return NULL;
            }
        }
    }

    return buf;
}
