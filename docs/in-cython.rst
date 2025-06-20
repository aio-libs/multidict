.. _cython-api:

==========
Cython API
==========

Multidicts implements a cython api that is used for speeding up the aiohttp http parser
but this feature can be used elsewhere in your own projects.


Introduction
------------
Multidict can be used with cython to speedup performance of other tools or 
scripts you may think about programming. Those who are familliar with the way 
`numpy <https://cython.readthedocs.io/en/latest/src/userguide/numpy_tutorial.html>`_
works should know that this library works the exact same way. If your not familliar with this don't worry. 

An example might be combining the node-js `llhttp <https://llhttp.org>`_ library 
and Multidict together for example (which is something aiohttp already 
does) . By using llhttp's callback functions on items such as HTTP headers and URL
query arguments you can build some extremely fast parsers and more 
with extra performance benefits included. 


Functions for using MultiDict in cython 
should have very simillar feel and format to the way CPython was written. e.g.:

.. code-block:: python

    from multidict cimport import_multidict, MultiDict, MultiDict_Add
    # always remeber to call import_multidict before anything else
    # otherwise your compilation will fail
    import_multidict()

    cdef MultiDict create_with_user_agent_header():
        cdef MultiDict md = MultiDict()
        MultiDict_Add(md, "user-agent", "Multidict-Made-User-Agent")
        return md






Compiling
---------
Compiling multidict with cython works the exact same way as *numpy* with the only
requirement being to link where the headers needed to compile the library are kept
luckily multidict includes a function to get where the headers are stored called 
*get_include* and it is no different from the way `numpy works <https://cython.readthedocs.io/en/latest/src/userguide/numpy_tutorial.html#compilation-using-setuptools>`_. 
e.g.:

.. code-block:: python

    from Cython.Build import cythonize
    from setuptools import Extension, setup
    import multidict

    if __name__ == "__main__":
        setup(
            ext_modules=cythonize(
                Extension(
                    "your_module.pyx", sources=["your_module.pyx"]
                )
            ),
            # in here is where you could but down your include directories
            include_dirs=[multidict.get_include()],
        )


Know that your are not limited to just one *include_dirs* directory in fact you could combine
*numpy* and *multidict* together if you really wanted to along with other C Libraries that you would
like to compile alongside it. e.g.:

.. code-block:: python

    from setuptools import Extension, setup
    from Cython.Build import cythonize
    import numpy
    import multidict

    extensions = [
        Extension("*", ["*.pyx"],
            include_dirs=[
                numpy.get_include(), 
                multidict.get_include(), 
                "my-other-clibraries/path/etc"
            ]
        ),
    ]
    setup(
        name="My hello app",
        ext_modules=cythonize(extensions),
    )

There's 





