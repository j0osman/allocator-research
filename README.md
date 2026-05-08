# Allocator Research (C++)

A research repository exploring custom C++ allocator designs and their behavior under low-latency, multi-threaded workloads.

The project focuses on:
- allocator architecture
- contention reduction
- cache locality
- deterministic latency behavior
- scalability under concurrent load

Implementations are benchmarked using:
- latency percentiles
- throughput
- multi-threaded stress testing
- hardware cache counters

---

# 🎯 Goals

This repository exists to study how allocator design choices affect:

- allocation latency
- tail latency stability
- cache behavior
- synchronization overhead
- scalability under contention
- memory reuse efficiency

The project compares:
- mutex-based freelists
- lock-free freelists
- thread-local allocator designs

while keeping implementation complexity and memory overhead practical.

---

# ⚙️ Implementations

## 1. Mutex Freelist Object Pool

A simple thread-safe slab allocator using a single global mutex.

### Characteristics

- contiguous slab allocation
- zero per-object metadata
- straightforward implementation
- predictable memory layout

### Tradeoffs

- performs reasonably in single-threaded workloads
- suffers under heavy contention
- mutex becomes serialization bottleneck

---

## 2. Lock-Free Freelist Object Pool

A lock-free freelist allocator using atomic CAS operations and tagged pointers to mitigate ABA issues.

### Characteristics

- mutex-free allocation path
- 64-bit packed tagged pointer freelist
- improved scalability over global mutex design

### Tradeoffs

- additional CAS overhead
- increased implementation complexity
- contention still visible under extreme thread pressure

---

## 3. TLS Freelist Object Pool

A hybrid allocator combining:
- thread-local freelists
- batched global refill
- slab allocation
- mutex-protected fallback storage

### Characteristics

- allocation fast path avoids synchronization entirely
- batch refill amortizes mutex overhead
- strong cache locality due to slab reuse
- deterministic low-latency allocation behavior
- alignment-safe allocations
- cross-thread release support

### Design Philosophy

This allocator prioritizes:
- latency determinism
- contention avoidance
- cache efficiency
- scalable throughput

rather than maximizing raw synthetic benchmark numbers.

---

# 🧠 Architectural Findings

The experiments in this repository demonstrate several recurring allocator behaviors:

## Mutex Contention Dominates Quickly

Global mutex designs degrade sharply once multiple threads begin competing for the freelist.

Even simple allocation patterns can become serialization bottlenecks under contention.

---

## Lock-Free Does Not Mean Contention-Free

Lock-free structures eliminate mutex blocking but still incur:
- cache coherency traffic
- CAS retry overhead
- atomic synchronization costs

Under heavy contention, atomic pressure becomes the new bottleneck.

---

## Thread-Local Allocation Dramatically Improves Stability

Keeping allocation/release operations local to each thread:
- minimizes synchronization
- reduces cacheline bouncing
- improves cache locality
- compresses latency tails significantly

The TLS allocator consistently demonstrated:
- lower p50 latency
- tighter p99/p999 distributions
- reduced L3 cache misses
- stronger multi-threaded scaling

---

# 📊 Benchmark Summary

Benchmarks performed on a pinned multi-core system using:

```bash
g++ -O3 -std=c++23
```

Metrics collected:
- latency percentiles
- throughput
- hardware cache counters

---

## Single-Threaded Results

| Allocator | p50 | p99 | Throughput |
|---|---|---|---|
| Mutex Pool | Moderate | Stable | Moderate |
| Lock-Free Pool | Higher | Stable | Moderate |
| TLS Pool | Lowest | Tightest | Highest |
| `new/delete` | Moderate | Variable | Moderate |

### Observations

- TLS freelists reduced median allocation latency substantially
- slab locality reduced allocator cache misses
- deterministic timing improved significantly versus heap allocation

---

## Multi-Threaded Results

| Allocator | Contention Behavior | Scaling |
|---|---|---|
| Mutex Pool | Severe mutex contention | Poor |
| Lock-Free Pool | Atomic contention | Moderate |
| TLS Pool | Minimal fast-path synchronization | Strong |
| `new/delete` | General-purpose allocator overhead | Moderate |

### Observations

- TLS batching avoided most synchronization entirely
- lock-free design scaled better than mutex-based freelists
- allocator locality strongly influenced tail latency stability

---

# 🧪 Validation & Testing

The repository includes extensive unit and stress testing covering:

- allocation correctness
- destructor symmetry
- pool exhaustion handling
- node reuse behavior
- multi-threaded stress tests
- alignment guarantees
- cross-thread frees
- multiple allocator instances
- concurrent exhaustion scenarios
- zero-sized type support

Tests validate:
- memory lifecycle correctness
- allocator reuse integrity
- thread safety assumptions
- deterministic object construction/destruction behavior

Example test output:

```text
==== ObjectPool Unit Tests ====

✓ basic allocate
✓ capacity
✓ reuse
✓ bulk allocate
✓ multithread
✓ alignment
✓ lifecycle
✓ cross-thread free
✓ multiple pools same thread
✓ multithread exhaustion
✓ empty type

All tests passed.
```

---

# 💻 Build

## Benchmarks

```bash
g++ -O3 -std=c++23 benchmark.cpp -pthread -o benchmark
./benchmark
```

## Unit Tests

```bash
g++ -O3 -std=c++23 tests.cpp -pthread -o tests
./tests
```

---

# 📌 Research Focus

This repository is primarily focused on studying allocator behavior in systems where:
- latency matters
- contention matters
- allocation patterns are repetitive
- cache locality is important

Potential application domains include:
- trading systems
- ECS/game engines
- simulation systems
- packet processing
- networking infrastructure
- real-time systems

---

# ⚠️ Safety Notes

These allocators manage raw slab memory directly.

Users must:
- avoid use-after-free access
- avoid retaining pointers after pool destruction
- avoid releasing objects into incompatible pools
- manage allocator lifetimes carefully

Improper memory ownership handling may result in undefined behavior.

---

# 📄 License

MIT License
