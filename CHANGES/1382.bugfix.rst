Stopped several feature-detection fallbacks in the C extension from swallowing every exception. When probing an argument (``arg.items()``/``arg.keys()``) or measuring it (``len(other)``) fails, the code now clears only the expected :exc:`TypeError`/:exc:`AttributeError` and lets everything else -- notably :exc:`MemoryError` and :exc:`KeyboardInterrupt` -- propagate, matching the already-correct sites elsewhere in the module.

-- by :user:`devdanzin`
