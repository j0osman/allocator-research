# High-Performance TLS/Mutex Hybrid C++ Object Pool

A header-only C++23 Object Pool implementation combining **thread-local storage (TLS)** and a **global mutex-protected freelist**. Designed for extreme multi-threaded throughput while retaining deterministic allocation timing.

This implementation demonstrates how combining **per-thread caches** with a shared global pool can drastically reduce contention while still using a slab allocation strategy.

## 🚀 Key Features

* **Thread-Local Fast Path**: Each thread has its own free list, drastically reducing the need for locking under normal usage.  
* **Global Mutex Fallback**: Threads pull batches of objects from a shared global pool when their local cache is empty.  
* **Zero-Metadata Per Object**: Uses a `union` to store either the object or the next-pointer, keeping memory usage minimal: `max(sizeof(T), sizeof(void*))`.  
* **Deterministic Latency**: `acquire` and `release` are fast and mostly O(1) thanks to TLS batching.  
* **Cache-Friendly**: Contiguous slab allocation keeps objects close in memory for high cache efficiency.  
* **RAII Compliant**: Objects are safely constructed and destructed using placement new and manual destructor calls.

## 🛠 Architecture

The pool maintains **two layers of freelists**:

1. **Thread-Local Free List** (`tls_free_head`):  
   * Fast path for acquire/release.  
   * Each thread maintains a small batch of pre-fetched nodes for minimal contention.  

2. **Global Free List** (`global_head`):  
   * Protected by a `std::mutex`.  
   * Threads refill their TLS cache in batches (`TLS_BATCH`) when empty.  
   * TLS caches flush back to the global pool when exceeding a threshold (`TLS_LIMIT`).

This hybrid strategy ensures that most operations avoid locking entirely, only touching the global pool when necessary.

## 📊 Benchmarks

Measured on an 8-thread system using `g++ -O2 -std=c++23`.

### Single-Threaded Performance

| Test Case | Time (s) | Throughput (ops/sec) |
| :--- | :--- | :--- |
| **Standard `new`/`delete`** | 0.092192 | 54,234,679 |
| **TLS/Mutex Hybrid Pool** | 0.010858 | 460,501,387 |

> **Observation:** In single-threaded mode, TLS batching results in ~8.5× speedup over standard heap allocation.

### Multi-Threaded Performance (8 Threads)

| Test Case | Time (s) | Throughput (ops/sec) |
| :--- | :--- | :--- |
| **Standard `new`/`delete`** | 0.158295 | 252,692,173 |
| **TLS/Mutex Hybrid Pool** | 0.021129 | 1,893,107,341 |

> **Observation:** Dramatic performance improvement due to reduced contention. TLS batching ensures most allocations never touch the global mutex, achieving nearly **7.5× throughput** over the standard allocator.

## TLS/Mutex Hybrid Advantages

* Combines the **scalability of lock-free per-thread caches** with the **simplicity of a mutex-protected global pool**.  
* Ideal for workloads with heavy multi-threaded allocation and release.  
* Maintains deterministic memory usage with minimal metadata overhead.  

## 💻 Usage

```bash
g++ -O2 -std=c++23 benchmark.cpp -o benchmark -pthread
./benchmark
```

### Requirements
* C++23 compatible compiler (GCC 13+, Clang 16+, or MSVC 19.30+)
* Pthread support (for multi-threaded benchmarks)

### Integration
Simply include the header-only library in your project:

```cpp
#include "object_pool.hpp"

struct Particle {
    float x, y, z;
};

int main() {
    // Initialize pool with 1,000 slots
    ObjectPool<Particle> pool(1000);

    // Acquire an object (calls constructor via placement new)
    Particle* p = pool.acquire();

    // Use object...
    p->x = 10.0f;

    // Release object (calls destructor and returns to freelist)
    pool.release(p);

    return 0;
}
