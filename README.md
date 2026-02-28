Allocator Research (C++)

Goal:
Explore how different allocator designs behave under contention.

Implementations:
1. Mutex freelist object pool
2. Lock-free freelist (128-bit CAS)
3. Multi-slab allocator
4. Lock-free slab allocator
5. Thread-local slab allocator

Benchmarks:
Throughput scaling
Allocation latency
Contention behavior
