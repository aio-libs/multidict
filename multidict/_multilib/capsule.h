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
	PyObject (*_MultiDict_Clear)(MultiDictObject *self);

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

} MultiDict_CAPI;

static MultiDict_CAPI *MultiDictAPI = NULL;

#define MultiDict_IMPORT \
	MultiDictAPI = PyCapsule_Import("multidict._multidict.multidict_CAPI", 0)

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

#define CIMultiDict_GetAll(self, key, ret) \
	MultiDictAPI->_MultiDict_GetAll(self, key, ret)

#define CIMultiDict_GetOne(self, key, ret) \
	MultiDictAPI->_MultiDict_GetOne(self, key, ret)

#define CIMultiDict_Keys(self) \
	MultiDictAPI->_MultiDict_Keys(self)

#define CIMutliDict_Values(self) \
	MultiDictAPI->_MultiDict_Values(self)

#define CIMutliDict_Items(self) \
	MultiDictAPI->_MultiDict_Items(self)

#define CIMultiDict_Add(self, key, value) \
	MultiDictAPI->_MultiDict_Add(self, key, value)

#define CIMutliDict_Clear(self) \
	MultiDictAPI->_MultiDict_Clear(self)

#define CIMultiDict_Extend(self, args, kwargs) \
	MultiDictAPI->_MultiDict_Extend(self, args, kwargs)

#define CIMultiDict_Copy(self) \
	MultiDictAPI->_MultiDict_Copy(self)

#define CIMultiDict_SetDefault(self, key, value) \
	MultiDictAPI->_MultiDict_SetDefault(self, key, value)

#define CIMultiDict_PopOne(self, key, ret) \
	MultiDictAPI->_MultiDict_PopOne(self, key, ret)

#define CIMultiDict_Update(self, args, kwargs) \
	MultiDictAPI->_MultiDict_Update(self, args, kwargs)

#ifdef __cplusplus
}
#endif

#endif // __CAPSULE_H__
