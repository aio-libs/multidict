#ifndef _MULTIDICT_C_H
#define _MULTIDICT_C_H

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {  // 16 or 24 for GC prefix
    PyObject_HEAD  // 16
    PyObject *weaklist;
    pair_list_t pairs;
} MultiDictObject;

typedef struct {
    PyObject_HEAD
    PyObject *weaklist;
    MultiDictObject *md;
} MultiDictProxyObject;


static inline PyObject *
_do_multidict_repr(MultiDictObject *md, PyObject *name,
                   bool show_keys, bool show_values);


#ifdef __cplusplus
}
#endif

#endif
