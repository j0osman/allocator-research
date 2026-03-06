Allocator Research (C++)

Goal:
Explore how different allocator designs behave under contention.

Implementations:
1. Mutex freelist object pool
2. Lock-free freelist (64-bit packed pointer CAS)
3. Thread-local freelist object pool
4. Thread-local multi-slab allocator

Benchmarks:
Throughput scaling
Allocation latency
Contention behavior
