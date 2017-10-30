import functools
import textwrap

import perf


IMPLEMENTATIONS = {
    'c': """\
    from multidict._multidict import istr
    """,
    'python': """\
    from multidict._multidict_py import istr
    """
}

INIT = """\
val = istr('VaLuE')
"""


ISTR_TO_ISTR = """\
istr(val)
istr(val)
istr(val)
istr(val)
istr(val)
istr(val)
istr(val)
istr(val)
istr(val)
istr(val)
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
    parser.description = ('Allows to measure performance of '
                          'istr implementations')
    parser.add_argument('--impl', choices=sorted(IMPLEMENTATIONS),
                        help='specific implementation to benchmark')

    options = parser.parse_args()
    implementations = (options.impl,) if options.impl else IMPLEMENTATIONS

    for name in implementations:
        imports = textwrap.dedent(IMPLEMENTATIONS[name])
        name = functools.partial(benchmark_name, ctx=dict(impl=name),
                                 prefix='(impl = %(impl)s) ',
                                 use_prefix=len(implementations) > 1)

        runner.timeit(name('istr->istr'),
                      ISTR_TO_ISTR, imports + INIT,
                      inner_loops=10)
