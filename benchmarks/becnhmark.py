import functools
import textwrap

import perf


IMPLEMENTATIONS = {
    'dict': """\
    cls = dict
    """,
    'multidict_cython': """\
    from multidict._multidict import MultiDict as cls, istr
    """,
    'multidict_python': """\
    from multidict._multidict_py import MultiDict as cls, istr
    """,
    'cimultidict_cython': """\
    from multidict._multidict import CIMultiDict as cls, istr
    """,
    'cimultidict_python': """\
    from multidict._multidict_py import CIMultiDict as cls, istr
    """,
}

INIT = """\
dct = cls()
"""

FILL = """\
for i in range(20):
    dct['key'+str(i)] = str(i)

key = 'key10'
"""


FILL_ISTR = """\
for i in range(20):
    key = istr('key'+str(i))
    dct[key] = str(i)

key = istr('key10')
"""


SET_ITEM = (
    """\
dct[key] = '1'
dct[key] = '2'
dct[key] = '3'
dct[key] = '4'
dct[key] = '5'
dct[key] = '6'
dct[key] = '7'
dct[key] = '8'
dct[key] = '9'
dct[key] = '10'
"""
    * 10
)


GET_ITEM = (
    """\
dct[key]
dct[key]
dct[key]
dct[key]
dct[key]
dct[key]
dct[key]
dct[key]
dct[key]
dct[key]
"""
    * 10
)


ADD = (
    """\
add(key, '1')
add(key, '2')
add(key, '3')
add(key, '4')
add(key, '5')
add(key, '6')
add(key, '7')
add(key, '8')
add(key, '9')
add(key, '10')
"""
    * 10
)

SETUP_ADD = """\
add = dct.add
"""


def benchmark_name(name, ctx, prefix=None, use_prefix=False):
    if use_prefix:
        return '%s%s' % (prefix % ctx, name)

    return name


def add_impl_option(cmd, args):
    if args.impl:
        cmd.extend(['--impl', args.impl])


if __name__ == '__main__':
    runner = perf.Runner(add_cmdline_args=add_impl_option)

    parser = runner.argparser
    parser.description = (
        'Allows to measure performance of MultiMapping and '
        'MutableMultiMapping implementations'
    )
    parser.add_argument(
        '--impl',
        choices=sorted(IMPLEMENTATIONS),
        help='specific implementation to benchmark',
    )

    options = parser.parse_args()
    implementations = (options.impl,) if options.impl else IMPLEMENTATIONS

    for impl in implementations:
        imports = textwrap.dedent(IMPLEMENTATIONS[impl])
        name = functools.partial(
            benchmark_name,
            ctx=dict(impl=impl),
            prefix='(impl = %(impl)s) ',
            use_prefix=len(implementations) > 1,
        )

        runner.timeit(
            name('setitem str'),
            SET_ITEM,
            imports + INIT + FILL,
            inner_loops=30,
        )
        runner.timeit(
            name('getitem str'),
            GET_ITEM,
            imports + INIT + FILL,
            inner_loops=30,
        )

        # MultiDict specific
        if impl == 'dict':
            continue

        runner.timeit(
            name('setitem istr'),
            SET_ITEM,
            imports + INIT + FILL,
            inner_loops=30,
        )
        runner.timeit(
            name('getitem istr'),
            GET_ITEM,
            imports + INIT + FILL,
            inner_loops=30,
        )
        runner.timeit(
            name('add str'),
            ADD,
            imports + INIT + FILL + SETUP_ADD,
            inner_loops=30,
        )
