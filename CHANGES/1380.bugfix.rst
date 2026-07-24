Fixed a reference leak where each :py:class:`~multidict.MultiDict`, :py:class:`~multidict.CIMultiDict`, :py:class:`~multidict.MultiDictProxy` and :py:class:`~multidict.CIMultiDictProxy` instance leaked a reference to its type. ``tp_dealloc`` now releases the type reference that heap-type instances hold, as the iterator, view and ``istr`` types already did.

While fixing this, a related use-after-free at interpreter shutdown was also fixed: ``md_clear()`` no longer bumps the version counter (which dereferences the per-module state) during ``tp_clear``/``tp_dealloc``, since the module state may already be freed by then. The version bump now happens in the user-facing ``clear()`` method, where the module is guaranteed to be alive.

-- by :user:`devdanzin`
