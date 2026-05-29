## Build

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
```

## Run

```bash
./build/cache_line_latency --cores 0,1,2,3 --iterations 100000 --warmup 10000
```

Arguments:

```text
--cores       comma-separated CPU ids
--iterations  measured iterations per CPU pair
--warmup      warmup iterations per CPU pair
```

## Notes

The benchmark assumes a synchronized invariant TSC across the selected cores and should be run on isolated physical cores.
