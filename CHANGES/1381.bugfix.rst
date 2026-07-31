Fixed three reference/resource leaks on error paths in the C extension: the items-view ``__contains__`` leaked the first element of a candidate pair when reading the second one raised; ``__repr__`` leaked its ``PyUnicodeWriter`` when the multidict was mutated during iteration; and the internal iteration helper leaked the identity reference when key materialisation failed under low memory.

-- by :user:`devdanzin`
