"""Multidict implementation.

HTTP Headers and URL query string require specific data structure:
multidict. It behaves mostly like a dict but it can have
several values for the same key.
"""

import os

__all__ = ('MultiDictProxy', 'CIMultiDictProxy',
           'MultiDict', 'CIMultiDict', 'upstr')

__version__ = '1.2.2'


if bool(os.environ.get('MULTIDICT_NO_EXTENSIONS')):
    from ._multidict_py import (MultiDictProxy,
                                CIMultiDictProxy,
                                MultiDict,
                                CIMultiDict,
                                upstr)
else:
    try:
        from ._multidict import (MultiDictProxy,
                                 CIMultiDictProxy,
                                 MultiDict,
                                 CIMultiDict,
                                 upstr)
    except ImportError:  # pragma: no cover
        from ._multidict_py import (MultiDictProxy,
                                    CIMultiDictProxy,
                                    MultiDict,
                                    CIMultiDict,
                                    upstr)
