#include <Python.h>

#include "multidict_capi.h"

/* Exercises the public C API capsule from the test suite. Not part of the
   public API and not meant to be imported or relied on outside tests. */

typedef struct {
    MultiDict_CAPI* capi;
    PyObject* log;   // mutating_event()'s log; see mutating_ctx below
    int unwatch_id;  // unwatching_event()'s own watcher id
    int watch_id;    // the watcher watching_event() attaches
    /* Keeps every object handed to the C API as a raw `void*` alive: the
       API stores those pointers without a reference, so the harness has
       to. Emptied by watch_release_refs(). */
    PyObject* refs;
} mod_state;

static inline mod_state*
get_mod_state(PyObject* mod)
{
    mod_state* state = (mod_state*)PyModule_GetState(mod);
    assert(state != NULL);
    return state;
}

/* module functions */

static PyObject*
istr_type(PyObject* self, PyObject* unused)
{
    mod_state* state = get_mod_state(self);
    return (PyObject*)IStr_GetType(state->capi);
}

static PyObject*
istr_from_unicode(PyObject* self, PyObject* arg)
{
    mod_state* state = get_mod_state(self);
    return IStr_FromUnicode(state->capi, arg);
}

static PyObject*
md_getversion(PyObject* self, PyObject* arg)
{
    mod_state* state = get_mod_state(self);
    uint64_t version = MultiDict_GetVersion(state->capi, arg);
    if (version == 0 && PyErr_Occurred()) {
        return NULL;
    }
    return PyLong_FromUnsignedLongLong(version);
}

static PyObject*
md_type(PyObject* self, PyObject* unused)
{
    mod_state* state = get_mod_state(self);
    return (PyObject*)MultiDict_GetType(state->capi);
}

static PyObject*
cimd_type(PyObject* self, PyObject* unused)
{
    mod_state* state = get_mod_state(self);
    return (PyObject*)CIMultiDict_GetType(state->capi);
}

static PyObject*
mdproxy_type(PyObject* self, PyObject* unused)
{
    mod_state* state = get_mod_state(self);
    return (PyObject*)MultiDictProxy_GetType(state->capi);
}

static PyObject*
cimdproxy_type(PyObject* self, PyObject* unused)
{
    mod_state* state = get_mod_state(self);
    return (PyObject*)CIMultiDictProxy_GetType(state->capi);
}

static PyObject*
md_new(PyObject* self, PyObject* arg)
{
    mod_state* state = get_mod_state(self);
    Py_ssize_t prealloc_size = PyLong_AsSsize_t(arg);
    if (prealloc_size == -1 && PyErr_Occurred()) {
        return NULL;
    }
    return MultiDict_New(state->capi, prealloc_size);
}

static PyObject*
cimd_new(PyObject* self, PyObject* arg)
{
    mod_state* state = get_mod_state(self);
    Py_ssize_t prealloc_size = PyLong_AsSsize_t(arg);
    if (prealloc_size == -1 && PyErr_Occurred()) {
        return NULL;
    }
    return CIMultiDict_New(state->capi, prealloc_size);
}

static PyObject*
mdproxy_new(PyObject* self, PyObject* arg)
{
    mod_state* state = get_mod_state(self);
    return MultiDictProxy_New(state->capi, arg);
}

static PyObject*
cimdproxy_new(PyObject* self, PyObject* arg)
{
    mod_state* state = get_mod_state(self);
    return CIMultiDictProxy_New(state->capi, arg);
}

static PyObject*
md_size(PyObject* self, PyObject* arg)
{
    mod_state* state = get_mod_state(self);
    Py_ssize_t size = MultiDict_Size(state->capi, arg);
    if (size < 0) {
        return NULL;
    }
    return PyLong_FromSsize_t(size);
}

static PyObject*
md_contains(PyObject* self, PyObject* const* args, Py_ssize_t nargs)
{
    if (nargs != 2) {
        PyErr_SetString(PyExc_TypeError,
                        "md_contains should be called with md and key");
        return NULL;
    }
    mod_state* state = get_mod_state(self);
    int ret = MultiDict_Contains(state->capi, args[0], args[1]);
    if (ret < 0) {
        return NULL;
    }
    return PyBool_FromLong(ret);
}

/* Both elements of the returned tuple mirror PyDict_GetItemRef /
   PyDict_SetDefaultRef's `int` return plus `PyObject **result`
   design: (found, value_or_None). `result` is consumed (its
   reference, if any, is transferred into the returned tuple). */

static PyObject*
handle_result(int ret, PyObject* result)
{
    PyObject* value = result != NULL ? result : Py_NewRef(Py_None);
    PyObject* found = PyBool_FromLong(ret);
    if (found == NULL) {
        Py_DECREF(value);
        return NULL;
    }
    PyObject* tuple = PyTuple_Pack(2, found, value);
    Py_DECREF(found);
    Py_DECREF(value);
    return tuple;
}

static PyObject*
md_getitem(PyObject* self, PyObject* const* args, Py_ssize_t nargs)
{
    if (nargs != 2) {
        PyErr_SetString(PyExc_TypeError,
                        "md_getitem should be called with md and key");
        return NULL;
    }
    mod_state* state = get_mod_state(self);
    PyObject* result = NULL;
    int ret = MultiDict_GetItem(state->capi, args[0], args[1], &result);
    if (ret < 0) {
        return NULL;
    }
    return handle_result(ret, result);
}

static PyObject*
md_add(PyObject* self, PyObject* const* args, Py_ssize_t nargs)
{
    if (nargs != 3) {
        PyErr_SetString(PyExc_TypeError,
                        "md_add should be called with md, key and value");
        return NULL;
    }
    mod_state* state = get_mod_state(self);
    if (MultiDict_Add(state->capi, args[0], args[1], args[2]) < 0) {
        return NULL;
    }
    Py_RETURN_NONE;
}

static PyObject*
md_clear(PyObject* self, PyObject* md)
{
    mod_state* state = get_mod_state(self);
    if (MultiDict_Clear(state->capi, md) < 0) {
        return NULL;
    }
    Py_RETURN_NONE;
}

static PyObject*
md_delitem(PyObject* self, PyObject* const* args, Py_ssize_t nargs)
{
    if (nargs != 2) {
        PyErr_SetString(PyExc_TypeError,
                        "md_delitem should be called with md and key");
        return NULL;
    }
    mod_state* state = get_mod_state(self);
    if (MultiDict_DelItem(state->capi, args[0], args[1]) < 0) {
        return NULL;
    }
    Py_RETURN_NONE;
}

static PyObject*
md_pop(PyObject* self, PyObject* const* args, Py_ssize_t nargs)
{
    if (nargs != 2) {
        PyErr_SetString(PyExc_TypeError,
                        "md_pop should be called with md and key");
        return NULL;
    }
    mod_state* state = get_mod_state(self);
    PyObject* result = NULL;
    int ret = MultiDict_Pop(state->capi, args[0], args[1], &result);
    if (ret < 0) {
        return NULL;
    }
    return handle_result(ret, result);
}

static PyObject*
md_setdefault(PyObject* self, PyObject* const* args, Py_ssize_t nargs)
{
    if (nargs != 2 && nargs != 3) {
        PyErr_SetString(PyExc_TypeError,
                        "md_setdefault should be called with md, key and "
                        "optional default");
        return NULL;
    }
    mod_state* state = get_mod_state(self);
    PyObject* result = NULL;
    // Omitting the default passes NULL, which the C API reads as None.
    PyObject* default_value = nargs == 3 ? args[2] : NULL;
    int ret = MultiDict_SetDefault(
        state->capi, args[0], args[1], default_value, &result);
    if (ret < 0) {
        return NULL;
    }
    return handle_result(ret, result);
}

static PyObject*
md_setitem(PyObject* self, PyObject* const* args, Py_ssize_t nargs)
{
    if (nargs != 3) {
        PyErr_SetString(PyExc_TypeError,
                        "md_setitem should be called with md, key and value");
        return NULL;
    }
    mod_state* state = get_mod_state(self);
    if (MultiDict_SetItem(state->capi, args[0], args[1], args[2]) < 0) {
        return NULL;
    }
    Py_RETURN_NONE;
}

typedef struct {
    PyObject* list;
    Py_ssize_t limit;  // < 0 means no limit
} visit_ctx;

typedef struct {
    void* capi;
    PyObject* md;
    PyObject* added;  // the key the visitor inserts, never the walked one
} mutate_ctx;

static int
collect_pair(void* user_data, PyObject* identity, Py_hash_t hash,
             PyObject* key, PyObject* value)
{
    visit_ctx* ctx = (visit_ctx*)user_data;
    PyObject* hash_obj = PyLong_FromSsize_t((Py_ssize_t)hash);
    if (hash_obj == NULL) {
        return -1;
    }
    PyObject* pair = PyTuple_Pack(4, identity, hash_obj, key, value);
    Py_DECREF(hash_obj);
    if (pair == NULL) {
        return -1;
    }
    int appended = PyList_Append(ctx->list, pair) == 0;
    Py_DECREF(pair);
    if (!appended) {
        return -1;
    }
    if (ctx->limit >= 0 && PyList_GET_SIZE(ctx->list) >= ctx->limit) {
        return 0;
    }
    return 1;
}

static PyObject*
md_foreach(PyObject* self, PyObject* const* args, Py_ssize_t nargs)
{
    if (nargs != 3) {
        PyErr_SetString(PyExc_TypeError,
                        "md_foreach should be called with md, key and limit");
        return NULL;
    }
    mod_state* state = get_mod_state(self);
    PyObject* key = args[1] == Py_None ? NULL : args[1];
    Py_ssize_t limit = PyLong_AsSsize_t(args[2]);
    if (limit == -1 && PyErr_Occurred()) {
        return NULL;
    }
    visit_ctx ctx = {PyList_New(0), limit};
    if (ctx.list == NULL) {
        return NULL;
    }
    Py_ssize_t count =
        MultiDict_ForEach(state->capi, args[0], key, collect_pair, &ctx);
    if (count < 0) {
        Py_DECREF(ctx.list);
        return NULL;
    }
    return ctx.list;
}

static int
raising_visitor(void* user_data, PyObject* identity, Py_hash_t hash,
                PyObject* key, PyObject* value)
{
    (void)user_data;
    (void)identity;
    (void)hash;
    (void)key;
    (void)value;
    PyErr_SetString(PyExc_RuntimeError, "boom from visitor");
    return -1;
}

/* Mutates `md` from inside the walk, which the walk must refuse. */
static int
mutating_visitor(void* user_data, PyObject* identity, Py_hash_t hash,
                 PyObject* key, PyObject* value)
{
    (void)identity;
    (void)hash;
    (void)key;
    (void)value;
    mutate_ctx* ctx = (mutate_ctx*)user_data;
    if (MultiDict_Add(ctx->capi, ctx->md, ctx->added, ctx->added) < 0) {
        return -1;
    }
    return 1;
}

static PyObject*
md_foreach_mutates(PyObject* self, PyObject* const* args, Py_ssize_t nargs)
{
    if (nargs != 3) {
        PyErr_SetString(
            PyExc_TypeError,
            "md_foreach_mutates should be called with md, key and added");
        return NULL;
    }
    mod_state* state = get_mod_state(self);
    PyObject* key = args[1] == Py_None ? NULL : args[1];
    mutate_ctx ctx = {state->capi, args[0], args[2]};
    if (MultiDict_ForEach(state->capi, args[0], key, mutating_visitor, &ctx) <
        0) {
        return NULL;
    }
    Py_RETURN_NONE;
}

static PyObject*
md_foreach_raises(PyObject* self, PyObject* arg)
{
    mod_state* state = get_mod_state(self);
    if (MultiDict_ForEach(state->capi, arg, NULL, raising_visitor, NULL) < 0) {
        return NULL;
    }
    Py_RETURN_NONE;
}

/* watchers */

static PyObject*
_or_none(PyObject* obj)
{
    return obj == NULL ? Py_None : obj;
}

/* `watcher_data` is the log list the events land in and `user_data` is
   whatever the test attached to that particular multidict, which is the
   pair the whole API exists for. */
static int
record_event(void* watcher_data, void* user_data,
             const MultiDict_WatchInfo* info)
{
    /* The one discrimination in here: on DEALLOCATED `self` is at
       refcount 0, so record its address instead of the object. */
    PyObject* self = info->event == MultiDict_EVENT_DEALLOCATED
                         ? PyLong_FromVoidPtr(info->self)
                         : Py_NewRef(info->self);
    if (self == NULL) {
        return -1;
    }
    PyObject* item = Py_BuildValue("(iNOOnOOO)",
                                   (int)info->event,
                                   self,
                                   (PyObject*)user_data,
                                   _or_none(info->identity),
                                   (Py_ssize_t)info->hash,
                                   _or_none(info->key),
                                   _or_none(info->value),
                                   _or_none(info->old_value));
    if (item == NULL) {
        return -1;
    }
    int ret = PyList_Append((PyObject*)watcher_data, item);
    Py_DECREF(item);
    return ret;
}

static int
failing_event(void* watcher_data, void* user_data,
              const MultiDict_WatchInfo* info)
{
    (void)user_data;
    (void)info;
    // records that it ran, so the test can tell "raised" from "never called"
    int ret = PyList_Append((PyObject*)watcher_data, Py_None);
    PyErr_SetString(PyExc_RuntimeError, "boom from watcher");
    return ret == 0 ? -1 : ret;
}

/* Mutates the multidict it is watching, from inside the delivery of that
   multidict's own events. Legal because delivery happens after the
   operation with no lock held; it stops after a couple of rounds so the
   flush's swap-and-recheck loop terminates. Its watcher_data is the
   harness's own mod_state, since it needs the capsule to call back in. */
#define MUTATING_WATCHER_ROUNDS 3

static int
mutating_event(void* watcher_data, void* user_data,
               const MultiDict_WatchInfo* info)
{
    (void)user_data;
    mod_state* state = (mod_state*)watcher_data;
    PyObject* kind = PyLong_FromLong((long)info->event);
    if (kind == NULL) {
        return -1;
    }
    int appended = PyList_Append(state->log, kind);
    Py_DECREF(kind);
    if (appended < 0) {
        return -1;
    }
    if (info->event != MultiDict_EVENT_ADDED ||
        PyList_GET_SIZE(state->log) >= MUTATING_WATCHER_ROUNDS) {
        return 0;
    }
    PyObject* again = PyUnicode_FromString("again");
    if (again == NULL) {
        return -1;
    }
    int ret = MultiDict_Add(state->capi, info->self, again, again);
    Py_DECREF(again);
    return ret;
}

/* Unwatches from inside the first event of a burst; nothing recorded by
   the same operation may reach it afterwards. Never sees DEALLOCATED,
   since it stops being a watcher before the multidict can die. */
static int
unwatching_event(void* watcher_data, void* user_data,
                 const MultiDict_WatchInfo* info)
{
    (void)user_data;
    mod_state* state = (mod_state*)watcher_data;
    PyObject* kind = PyLong_FromLong((long)info->event);
    if (kind == NULL) {
        return -1;
    }
    int appended = PyList_Append(state->log, kind);
    Py_DECREF(kind);
    if (appended < 0) {
        return -1;
    }
    return MultiDict_Unwatch(state->capi, state->unwatch_id, info->self);
}

static PyObject*
md_add_unwatching_watcher(PyObject* self, PyObject* arg)
{
    mod_state* state = get_mod_state(self);
    Py_XSETREF(state->log, Py_NewRef(arg));
    int watcher_id =
        MultiDict_AddWatcher(state->capi, unwatching_event, state);
    if (watcher_id < 0) {
        return NULL;
    }
    state->unwatch_id = watcher_id;
    return PyLong_FromLong(watcher_id);
}

/* Attaches another, already registered watcher from inside delivery.
   Watching takes effect from the next event on, so that watcher joins
   part-way through the burst; see docs/capi.rst on bracket pairing. */
static int
watching_event(void* watcher_data, void* user_data,
               const MultiDict_WatchInfo* info)
{
    (void)user_data;
    mod_state* state = (mod_state*)watcher_data;
    PyObject* kind = PyLong_FromLong((long)info->event);
    if (kind == NULL) {
        return -1;
    }
    int appended = PyList_Append(state->log, kind);
    Py_DECREF(kind);
    if (appended < 0) {
        return -1;
    }
    /* Its own log as the attached watcher's user_data: record_event()
       puts user_data in the tuple, so it has to be a live object. */
    return MultiDict_Watch(
        state->capi, state->watch_id, info->self, state->log);
}

static PyObject*
md_add_watching_watcher(PyObject* self, PyObject* args)
{
    PyObject* log;
    int target_id;
    if (!PyArg_ParseTuple(args, "Oi", &log, &target_id)) {
        return NULL;
    }
    mod_state* state = get_mod_state(self);
    Py_XSETREF(state->log, Py_NewRef(log));
    state->watch_id = target_id;
    int watcher_id = MultiDict_AddWatcher(state->capi, watching_event, state);
    if (watcher_id < 0) {
        return NULL;
    }
    return PyLong_FromLong(watcher_id);
}

static PyObject*
md_add_mutating_watcher(PyObject* self, PyObject* arg)
{
    mod_state* state = get_mod_state(self);
    Py_XSETREF(state->log, Py_NewRef(arg));
    int watcher_id = MultiDict_AddWatcher(state->capi, mutating_event, state);
    if (watcher_id < 0) {
        return NULL;
    }
    return PyLong_FromLong(watcher_id);
}

static int
_watch_keep_alive(mod_state* state, PyObject* obj)
{
    return PyList_Append(state->refs, obj);
}

static PyObject*
_md_add_watcher(PyObject* self, PyObject* arg, MultiDict_WatchCallback cb)
{
    mod_state* state = get_mod_state(self);
    if (_watch_keep_alive(state, arg) < 0) {
        return NULL;
    }
    int watcher_id = MultiDict_AddWatcher(state->capi, cb, arg);
    if (watcher_id < 0) {
        return NULL;
    }
    return PyLong_FromLong(watcher_id);
}

static PyObject*
md_add_watcher(PyObject* self, PyObject* arg)
{
    return _md_add_watcher(self, arg, record_event);
}

static PyObject*
md_add_failing_watcher(PyObject* self, PyObject* arg)
{
    return _md_add_watcher(self, arg, failing_event);
}

static PyObject*
md_add_null_watcher(PyObject* self, PyObject* unused)
{
    (void)unused;
    mod_state* state = get_mod_state(self);
    if (MultiDict_AddWatcher(state->capi, NULL, NULL) < 0) {
        return NULL;
    }
    Py_RETURN_NONE;
}

static PyObject*
md_clear_watcher(PyObject* self, PyObject* arg)
{
    mod_state* state = get_mod_state(self);
    int watcher_id = (int)PyLong_AsLong(arg);
    if (watcher_id == -1 && PyErr_Occurred()) {
        return NULL;
    }
    if (MultiDict_ClearWatcher(state->capi, watcher_id) < 0) {
        return NULL;
    }
    Py_RETURN_NONE;
}

static PyObject*
md_watch(PyObject* self, PyObject* const* args, Py_ssize_t nargs)
{
    if (nargs != 3) {
        PyErr_SetString(
            PyExc_TypeError,
            "md_watch should be called with watcher_id, md and user_data");
        return NULL;
    }
    mod_state* state = get_mod_state(self);
    int watcher_id = (int)PyLong_AsLong(args[0]);
    if (watcher_id == -1 && PyErr_Occurred()) {
        return NULL;
    }
    if (_watch_keep_alive(state, args[2]) < 0) {
        return NULL;
    }
    if (MultiDict_Watch(state->capi, watcher_id, args[1], args[2]) < 0) {
        return NULL;
    }
    Py_RETURN_NONE;
}

static PyObject*
md_unwatch(PyObject* self, PyObject* const* args, Py_ssize_t nargs)
{
    if (nargs != 2) {
        PyErr_SetString(PyExc_TypeError,
                        "md_unwatch should be called with watcher_id and md");
        return NULL;
    }
    mod_state* state = get_mod_state(self);
    int watcher_id = (int)PyLong_AsLong(args[0]);
    if (watcher_id == -1 && PyErr_Occurred()) {
        return NULL;
    }
    if (MultiDict_Unwatch(state->capi, watcher_id, args[1]) < 0) {
        return NULL;
    }
    Py_RETURN_NONE;
}

static PyObject*
watch_release_refs(PyObject* self, PyObject* unused)
{
    (void)unused;
    mod_state* state = get_mod_state(self);
    if (PyList_SetSlice(state->refs, 0, PyList_GET_SIZE(state->refs), NULL) <
        0) {
        return NULL;
    }
    Py_RETURN_NONE;
}

static PyObject*
check_api_version(PyObject* self, PyObject* arg)
{
    long fake_version = PyLong_AsLong(arg);
    if (fake_version == -1 && PyErr_Occurred()) {
        return NULL;
    }
    mod_state* state = get_mod_state(self);
    /* A stack copy so corrupting api_version can't affect the real,
       shared capsule used by every other test. */
    MultiDict_CAPI fake_capi = *state->capi;
    fake_capi.api_version = (int)fake_version;
    if (_MultiDict_CheckAPIVersion(&fake_capi) < 0) {
        return NULL;
    }
    Py_RETURN_NONE;
}

/* module slots */

static int
module_traverse(PyObject* mod, visitproc visit, void* arg)
{
    Py_VISIT(get_mod_state(mod)->refs);
    Py_VISIT(get_mod_state(mod)->log);
    return 0;
}

static int
module_clear(PyObject* mod)
{
    Py_CLEAR(get_mod_state(mod)->refs);
    Py_CLEAR(get_mod_state(mod)->log);
    return 0;
}

static void
module_free(void* mod)
{
    (void)module_clear((PyObject*)mod);
}

static PyMethodDef module_methods[] = {
    {"istr_type", (PyCFunction)istr_type, METH_NOARGS},
    {"istr_from_unicode", (PyCFunction)istr_from_unicode, METH_O},
    {"md_getversion", (PyCFunction)md_getversion, METH_O},
    {"md_type", (PyCFunction)md_type, METH_NOARGS},
    {"cimd_type", (PyCFunction)cimd_type, METH_NOARGS},
    {"mdproxy_type", (PyCFunction)mdproxy_type, METH_NOARGS},
    {"cimdproxy_type", (PyCFunction)cimdproxy_type, METH_NOARGS},
    {"md_new", (PyCFunction)md_new, METH_O},
    {"cimd_new", (PyCFunction)cimd_new, METH_O},
    {"mdproxy_new", (PyCFunction)mdproxy_new, METH_O},
    {"cimdproxy_new", (PyCFunction)cimdproxy_new, METH_O},
    {"md_size", (PyCFunction)md_size, METH_O},
    {"md_contains", (PyCFunction)md_contains, METH_FASTCALL},
    {"md_getitem", (PyCFunction)md_getitem, METH_FASTCALL},
    {"md_add", (PyCFunction)md_add, METH_FASTCALL},
    {"md_clear", (PyCFunction)md_clear, METH_O},
    {"md_delitem", (PyCFunction)md_delitem, METH_FASTCALL},
    {"md_pop", (PyCFunction)md_pop, METH_FASTCALL},
    {"md_setdefault", (PyCFunction)md_setdefault, METH_FASTCALL},
    {"md_setitem", (PyCFunction)md_setitem, METH_FASTCALL},
    {"md_foreach", (PyCFunction)md_foreach, METH_FASTCALL},
    {"md_foreach_mutates", (PyCFunction)md_foreach_mutates, METH_FASTCALL},
    {"md_foreach_raises", (PyCFunction)md_foreach_raises, METH_O},
    {"md_add_watcher", (PyCFunction)md_add_watcher, METH_O},
    {"md_add_failing_watcher", (PyCFunction)md_add_failing_watcher, METH_O},
    {"md_add_mutating_watcher", (PyCFunction)md_add_mutating_watcher, METH_O},
    {"md_add_unwatching_watcher",
     (PyCFunction)md_add_unwatching_watcher,
     METH_O},
    {"md_add_watching_watcher",
     (PyCFunction)md_add_watching_watcher,
     METH_VARARGS},
    {"md_add_null_watcher", (PyCFunction)md_add_null_watcher, METH_NOARGS},
    {"md_clear_watcher", (PyCFunction)md_clear_watcher, METH_O},
    {"md_watch", (PyCFunction)md_watch, METH_FASTCALL},
    {"md_unwatch", (PyCFunction)md_unwatch, METH_FASTCALL},
    {"watch_release_refs", (PyCFunction)watch_release_refs, METH_NOARGS},
    {"check_api_version", (PyCFunction)check_api_version, METH_O},
    {NULL, NULL} /* sentinel */
};

static int
module_exec(PyObject* mod)
{
    mod_state* state = get_mod_state(mod);
    state->capi = MultiDict_GetCAPI();
    if (state->capi == NULL) {
        return -1;
    }
    state->refs = PyList_New(0);
    if (state->refs == NULL) {
        return -1;
    }
    if (PyModule_AddIntMacro(mod, MULTIDICT_MAX_WATCHERS) < 0) {
        return -1;
    }
    if (PyModule_AddIntMacro(mod, MultiDict_EVENT_ADDED) < 0) {
        return -1;
    }
    if (PyModule_AddIntMacro(mod, MultiDict_EVENT_REPLACED) < 0) {
        return -1;
    }
    if (PyModule_AddIntMacro(mod, MultiDict_EVENT_DELETED) < 0) {
        return -1;
    }
    if (PyModule_AddIntMacro(mod, MultiDict_EVENT_CLEARED) < 0) {
        return -1;
    }
    if (PyModule_AddIntMacro(mod, MultiDict_EVENT_CLONED) < 0) {
        return -1;
    }
    if (PyModule_AddIntMacro(mod, MultiDict_EVENT_DEALLOCATED) < 0) {
        return -1;
    }
    if (PyModule_AddIntMacro(mod, MultiDict_EVENT_BATCH_BEGIN) < 0) {
        return -1;
    }
    if (PyModule_AddIntMacro(mod, MultiDict_EVENT_BATCH_END) < 0) {
        return -1;
    }
    if (PyModule_AddIntMacro(mod, MultiDict_EVENT_LOST) < 0) {
        return -1;
    }
    return 0;
}

static struct PyModuleDef_Slot module_slots[] = {
    {Py_mod_exec, module_exec},
#if PY_VERSION_HEX >= 0x030c00f0
    {Py_mod_multiple_interpreters, Py_MOD_PER_INTERPRETER_GIL_SUPPORTED},
#endif
#if PY_VERSION_HEX >= 0x030d00f0
    {Py_mod_gil, Py_MOD_GIL_NOT_USED},
#endif
    {0, NULL},
};

static PyModuleDef testcapi_module = {
    .m_base = PyModuleDef_HEAD_INIT,
    .m_name = "_testcapi",
    .m_size = sizeof(mod_state),
    .m_methods = module_methods,
    .m_slots = module_slots,
    .m_traverse = module_traverse,
    .m_clear = module_clear,
    .m_free = (freefunc)module_free,
};

PyMODINIT_FUNC
PyInit__testcapi(void)
{
    return PyModuleDef_Init(&testcapi_module);
}
