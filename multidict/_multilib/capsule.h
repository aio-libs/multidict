#ifndef __CAPSULE_H__
#define __CAPSULE_H__

#include "Python.h"
#include "dict.h"

// Incase this gets used for C++ stuff here's a macro for it
#ifdef __cplusplus
extern "C"
{
#endif

typedef struct _multidict_capi {

	// md_get_all
	int (*_MultiDict_GetAll)(MultiDictObject *self, PyObject *key, PyObject **ret);
	// md_get_one
	int (*_MultiDict_GetOne)(MultiDictObject *self, PyObject *key, PyObject **ret);

	// multidict_keys
	PyObject *(*_MultiDict_Keys)(MultiDictObject *self);

	// multidict_items
	PyObject *(*_MultiDict_Items)(MultiDictObject *self);

	// multidict_values
	PyObject *(*_MultiDict_Values)(MultiDictObject *self);

	// md_add
	int (*_MultiDict_Add)(MultiDictObject *self, PyObject *key, PyObject *value);

	// multidict_clear
	PyObject *(*_MultiDict_Clear)(MultiDictObject *self);

	// multidict_extend
	PyObject *(*_MultiDict_Extend)(MultiDictObject *self, PyObject *args, PyObject *kwargs);

	// mutlidict_copy
	PyObject *(*_MultiDict_Copy)(MultiDictObject *self);

	// multidict_setdefault
	PyObject *(*_MultiDict_SetDefault)(MultiDictObject *self, PyObject *key, PyObject *value);

	// md_pop_one
	int (*_MultiDict_PopOne)(MultiDictObject *self, PyObject *key, PyObject **ret);

	// md_pop_all
	int (*_MultiDict_PopAll)(MultiDictObject *self, PyObject *key, PyObject **ret);

	// md_pop_item
	PyObject *(*_MultiDict_PopItem)(MultiDictObject *self);

	// multidict_update
	PyObject *(*_MultiDict_Update)(MultiDictObject *self, PyObject *args, PyObject *kwds);

	// 
	PyObject* (*_MultiDict_Delete)(MultiDictObject* self, PyObject* );
	
} MultiDict_CAPI;

static MultiDict_CAPI *MultiDictAPI = NULL;

int MultiDict_IMPORT() {
	MultiDictAPI = PyCapsule_Import("multidict._multidict.multidict_CAPI", 0);
	return (MultiDictAPI != NULL) ? 0 : -1;
}

/*********************  MultiDict / CIMultiDict Macros  *********************/

#define MultiDict_GetAll(self, key, ret) \
	MultiDictAPI->_MultiDict_GetAll(self, key, ret)

#define MultiDict_GetOne(self, key, ret) \
	MultiDictAPI->_MultiDict_GetOne(self, key, ret)

#define MultiDict_Keys(self) \
	MultiDictAPI->_MultiDict_Keys(self)

#define MutliDict_Values(self) \
	MultiDictAPI->_MultiDict_Values(self)

#define MutliDict_Items(self) \
	MultiDictAPI->_MultiDict_Items(self)

#define MultiDict_Add(self, key, value) \
	MultiDictAPI->_MultiDict_Add(self, key, value)

#define MutliDict_Clear(self) \
	MultiDictAPI->_MultiDict_Clear(self)

#define MultiDict_Extend(self, args, kwargs) \
	MultiDictAPI->_MultiDict_Extend(self, args, kwargs)

#define MultiDict_Copy(self) \
	MultiDictAPI->_MultiDict_Copy(self)

#define MultiDict_SetDefault(self, key, value) \
	MultiDictAPI->_MultiDict_SetDefault(self, key, value)

#define MultiDict_PopOne(self, key, ret) \
	MultiDictAPI->_MultiDict_PopOne(self, key, ret)

#define MultiDict_Update(self, args, kwargs) \
	MultiDictAPI->_MultiDict_Update(self, args, kwargs)


/*********************  MultiDictProxy / CIMultiDictProxy Macros  *********************/

#define MultiDictProxy_GetAll(self, key, ret) \
	MultiDictAPI->_MultiDict_GetAll(self->md, key, ret)

#define MultiDictProxy_GetOne(self, key, ret) \
	MultiDictAPI->_MultiDict_GetOne(self->md, key, ret)

// NOTE: MultiDictProxy_Get will be going in the __init__.pxd file

#define MultiDictProxy_Keys(self, key) \
	MultiDictAPI->_MultiDict_Keys(self->md, key)

#define MutliDictProxy_Values(self) \
	MultiDictAPI->_MultiDict_Values(self->md)

#define MutliDictProxy_Items(self) \
	MultiDictAPI->_MultiDict_Items(self->md)

#define MultiDictProxy_Copy(self) \
	MultiDictAPI->_MultiDict_Copy(self->md)



// TODO: (Vizonex) I would like to add IStr next
// so we can speed up aiohttp's http-writer


#ifdef __cplusplus
}
#endif

#endif // __CAPSULE_H__
