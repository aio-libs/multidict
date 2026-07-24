Fixed two crashes in the C extension caused by holding a raw pointer into a hash table across an operation that could reshape it. Updating a multidict from itself (e.g. ``d.extend(d)``) freed the very table being iterated -- a use-after-free; ``extend(self)`` now doubles the contents and ``update(self)``/``merge(self)`` are no-ops. Building a multidict from a list of pairs whose case-insensitive key ``.lower()`` shrinks that list read past the end of the list; the length is now re-checked on every iteration.

-- by :user:`devdanzin`
