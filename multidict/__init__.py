"""Multidict implementation.

HTTP Headers and URL query string require specific data structure:
multidict. It behaves mostly like a dict but it can have
several values for the same key.
"""

import os

__all__ = ('MultiDictProxy', 'CIMultiDictProxy',
           'MultiDict', 'CIMultiDict', 'upstr', 'istr')

__version__ = '3.1.3'


if bool(os.environ.get('MULTIDICT_NO_EXTENSIONS')):
    from ._multidict_py import (MultiDictProxy,
                                CIMultiDictProxy,
                                MultiDict,
                                CIMultiDict,
                                upstr, istr)
else:
    try:
        from ._multidict import (MultiDictProxy,
                                 CIMultiDictProxy,
                                 MultiDict,
                                 CIMultiDict,
                                 upstr, istr)
    except ImportError:  # pragma: no cover
        from ._multidict_py import (MultiDictProxy,
                                    CIMultiDictProxy,
                                    MultiDict,
                                    CIMultiDict,
                                    upstr, istr)
