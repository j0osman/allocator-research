# High-Performance TLS Object Pool for C++23

A header-only C++23 object pool optimized for low-latency multi-threaded workloads using:

- thread-local freelists
- batched global refill
- slab allocation
- cache-friendly memory reuse

Designed for systems where allocator determinism and contention reduction matter more than generic heap flexibility.

---

# 🚀 Features

- Fast thread-local allocation path
- Batched synchronization to minimize mutex contention
- Deterministic allocation behavior with tight tail latency
- Slab-based contiguous allocation for cache locality
- Minimal per-node metadata overhead
- RAII-safe construction/destruction
- Cross-thread release support
- Alignment-safe allocations
- Header-only C++23 implementation

---

# 🛠 Architecture

The allocator uses a hybrid two-level freelist design.

## Thread-Local Fast Path

Each thread maintains its own freelist cache:

```text
Thread
  └── TLS freelist
```

Most `acquire()` and `release()` operations complete without synchronization.

This minimizes:
- mutex contention
- cacheline bouncing
- allocator metadata traffic

---

## Global Refill Path

When a thread exhausts its local cache:
- a batch of nodes is transferred from the global freelist
- synchronization occurs only during refill/flush operations

This amortizes synchronization cost across many allocations.

---

## Slab Allocation

Objects are allocated from contiguous slabs:

- improved cache locality
- reduced heap fragmentation
- fewer system allocator interactions
- predictable memory layout

---

# 📊 Benchmark Results

Benchmarks performed on a pinned 4-core system using:

```bash
g++ -O3 -std=c++23
```

Metrics collected:
- latency percentiles
- throughput
- hardware cache counters

---

## Single-Threaded Performance

| Allocator | p50 | p99 | p999 | Throughput |
|---|---|---|---|---|
| `new/delete` | 63 cycles | 136 cycles | 144–282 cycles | ~17 Mops/s |
| TLS Pool | 41 cycles | 43–51 cycles | 48–55 cycles | ~24 Mops/s |

### Key Observations

- ~40% lower median latency
- dramatically tighter tail latency
- significantly fewer L3 cache misses
- more deterministic allocation timing

---

## Multi-Threaded Performance (4 Threads)

| Allocator | p50 | p99 | p999 | Throughput |
|---|---|---|---|---|
| `new/delete` | 67 cycles | 138–144 cycles | 160–215 cycles | ~60 Mops/s |
| TLS Pool | 41–42 cycles | 47–53 cycles | 51–81 cycles | ~83–86 Mops/s |

### Key Observations

- strong multi-threaded scaling
- minimal contention on the fast path
- reduced cache coherency overhead
- stable latency under concurrent load

---

# 🧪 Unit Testing

The allocator includes a comprehensive unit test suite covering:

- basic allocation/release correctness
- pool exhaustion handling
- freed-node reuse behavior
- bulk allocation patterns
- multi-threaded stress testing
- lifecycle symmetry
- alignment guarantees
- cross-thread frees
- multiple pools within the same thread
- concurrent exhaustion scenarios
- empty/zero-sized type support

The tests validate:
- constructor/destructor correctness
- memory lifecycle integrity
- thread safety
- alignment correctness
- allocator reuse behavior

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

# 💻 Usage

```cpp
#include "object_pool.hpp"

struct Particle {
    float x, y, z;
};

int main() {
    // Create pool with 1024 slots
    ObjectPool<Particle> pool(1024);

    // Acquire object
    Particle* p = pool.acquire();

    p->x = 10.0f;

    // Release object back to pool
    pool.release(p);

    return 0;
}
```

---

# 🔨 Build

## Benchmark

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

# 📌 Design Goals

This allocator prioritizes:

- low-latency allocation
- deterministic timing behavior
- contention avoidance
- cache efficiency
- scalable multi-threaded performance

It is well suited for:
- trading systems
- ECS/game engines
- simulation systems
- packet processing
- high-frequency object reuse workloads
- low-latency infrastructure

---

# ⚠ Safety Notes

The pool owns all slab memory for its lifetime.

Do not:
- retain pointers after pool destruction
- access released objects
- release objects into the wrong pool
- use pool memory outside allocator lifetime

Improper lifetime management may result in undefined behavior or use-after-free errors.

---

# 📄 License

MIT License
