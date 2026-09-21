Fixed a reference leak in the C extension where ``operand | md.items()``
and ``md.items() - operand`` leaked one key and one value reference per
element of ``operand``, letting a large operand grow memory without bound
(:gh:`GHSA-54p9-h82j-f925 <aio-libs/multidict/security/advisories/GHSA-54p9-h82j-f925>`)
-- by :user:`asvetlov`.

The issue was reported by :user:`waydeshi`.
