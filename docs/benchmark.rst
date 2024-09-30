# Benchmarks

## Introduction

Benchmarks are essential for tracking performance across releases and ensuring that recent changes haven't significantly impacted it. The multidict library uses [pyperf](https://pyperf.readthedocs.io/en/latest/) for benchmarking.

## Prerequisites

Before running benchmarks:

1. Install the development requirements:
   ```
   pip install -r requirements/dev.txt
   ```

2. Configure your OS for reliable benchmark results as described in the [pyperf system configuration guide](https://pyperf.readthedocs.io/en/latest/system.html).

## Running Benchmarks

### Basic Usage

To run all benchmarks:

```bash
python benchmarks/benchmark.py
```

This command runs benchmarks for both `MultiDict` and `CIMultiDict` classes in both pure-Python and C implementations.

### Specific Implementation

To benchmark a specific class implementation, use the `--impl` option:

```bash
python benchmarks/benchmark.py --impl multidict_c
```

This runs benchmarks only for the C implementation of `MultiDict`.

### Additional Options

Run `python benchmarks/benchmark.py --help` to see all available options. Most options are described in the [pyperf Runner documentation](https://pyperf.readthedocs.io/en/latest/runner.html).

## Comparing Implementations

To compare different implementations:

1. Run benchmarks for each implementation:
   ```bash
   python benchmarks/benchmark.py --impl multidict_c -o multidict_c.json
   python benchmarks/benchmark.py --impl multidict_py -o multidict_py.json
   ```

2. Compare the results:
   ```bash
   python -m perf compare_to multidict_c.json multidict_py.json
   ```

This comparison provides insights into the performance differences between implementations.

## Best Practices

1. Run benchmarks on a quiet system to minimize interference from other processes.
2. Perform multiple runs to account for system variability.
3. Use the same hardware and system configuration when comparing benchmarks over time.
4. Document the system specifications and configuration used for each benchmark run.

## Interpreting Results

When analyzing benchmark results:

1. Look for significant changes in performance metrics.
2. Consider the context of changes (e.g., new features, optimizations).
3. Investigate any unexpected performance regressions.
4. Use statistical measures provided by pyperf to assess the reliability of results.

## Continuous Integration

Consider integrating benchmark runs into your CI/CD pipeline to catch performance regressions early. Set up alerts for significant changes in benchmark results.

## Contributing

When contributing performance improvements:

1. Run benchmarks before and after your changes.
2. Include benchmark results in your pull request.
3. Explain the performance impact of your changes.

By following these guidelines, you can maintain and improve the performance of the multidict library over time.
