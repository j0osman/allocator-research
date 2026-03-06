# High-Performance C++ Object Pool

A thread-safe, header-only C++23 Object Pool implementation designed to eliminate heap fragmentation and provide deterministic allocation timing. This project demonstrates the transition from standard dynamic memory management to a low-level **Slab Allocation** strategy using a **Union-based Freelist**.

## 🚀 Key Features

* **Zero-Metadata Per Object**: Utilizes a `union` to store either the object or the next-pointer, ensuring the memory footprint is exactly `max(sizeof(T), sizeof(void*))`.
* **Deterministic Latency**: Bypasses the OS heap manager for `acquire` and `release` operations, providing $O(1)$ complexity.
* **Cache Locality**: Allocates a single contiguous "slab" of memory, keeping objects physically close to one another to improve L1/L2 cache hit rates.
* **RAII Compliant**: Uses `std::lock_guard` for exception-safe thread synchronization and automatic resource cleanup.

## 🛠 Architecture

The pool operates on a **Freelist** principle. Upon construction, a single large block of raw memory is allocated. This block is partitioned into a linked list of "nodes."

* **When free**: The node acts as a pointer to the next available slot.  
* **When acquired**: The node is "reincarnated" into a live object using **Placement New**.  
* **When released**: The object's destructor is called manually, and the node is prepended back to the freelist.

## 📊 Benchmarks

Measured on an 8-thread system using `g++ -O2 -std=c++23`.

### Single-Threaded Performance

| Test Case | Time (s) | Throughput (ops/sec) |
| :--- | :--- | :--- |
| **Standard `new`/`delete`** | 0.085243 | 58,656,034 |
| **ObjectPool<T>** | 0.088398 | 56,562,397 |

> **Observation:** In single-threaded usage, the pool performs nearly on par with the standard heap allocator.

### Multi-Threaded Performance (8 Threads)

| Test Case | Time (s) | Throughput (ops/sec) |
| :--- | :--- | :--- |
| **Standard `new`/`delete`** | 0.196831 | 203,220,022 |
| **ObjectPool<T>** | 13.757565 | 2,907,491 |

> **Observation:** The mutex-based pool suffers from severe contention under heavy multi-threaded load, resulting in a drastic throughput drop.

### The "Mutex Tax" Analysis

* The single `std::mutex` protecting the freelist becomes a bottleneck with multiple threads.  
* Standard `new`/`delete` implementations leverage multiple arenas and thread-local caches to avoid such contention.  

*Note:* A **Lock-Free (Atomic CAS)** version is planned to eliminate this bottleneck and improve scalability.

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
