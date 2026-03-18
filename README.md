# Allocator Research (C++)

A research repository exploring **custom C++ memory allocator designs** and their performance under single-threaded and multi-threaded workloads. The goal is to understand how allocator design choices affect **throughput, latency, cache behavior, and contention**.

---

## 🏁 Goal

* Investigate different allocator strategies for high-performance, multi-threaded applications.
* Compare mutex-based, lock-free, and thread-local approaches to memory management.
* Quantify performance trade-offs with micro-benchmarks and stress tests.

---

## ⚙️ Implementations

1. **Mutex Freelist Object Pool**  
   * Thread-safe via a single `std::mutex`.  
   * Simple, zero-metadata per-object freelist using a union.  
   * Good single-threaded performance; suffers under multi-threaded contention.  

2. **Lock-Free Freelist (64-bit Packed Pointer CAS)**  
   * Uses atomic operations and a 64-bit **tagged pointer** to prevent ABA issues.  
   * Eliminates mutex bottlenecks, providing better scalability in multi-threaded scenarios.  
   * Slight overhead in single-threaded mode due to CAS operations.

3. **Enterprise Thread-Local Freelist Object Pool (Available via Commercial Licensing)**  
   * Combines **thread-local caches** with a global mutex-protected pool. 
   * Best multi-threaded throughput among the three designs with 1.8 billion ops/sec.

---

## 📊 Benchmarks

Benchmarks measured on an 8-thread system using `g++ -O2 -std=c++23`. Metrics include **time**, **throughput**, and scaling behavior.

### Single-Threaded Performance

| Allocator | Time (s) | Throughput (ops/sec) | Notes |
| :--- | :--- | :--- | :--- |
| Mutex Pool | 0.0907 | 56,562,397 | Slightly faster than standard heap in some cases |
| Lock-Free Pool | 0.1052 | 47,518,088 | CAS overhead visible |
| TLS/Mutex Hybrid | 0.0109 | 460,501,387 | TLS batching provides massive speedup |
| `new/delete` | 0.0852 | 58,656,034 | Baseline |

### Multi-Threaded Performance (8 Threads)

| Allocator | Time (s) | Throughput (ops/sec) | Notes |
| :--- | :--- | :--- | :--- |
| Mutex Pool | 10.2785 | 2,907,491 | Contention bottleneck on mutex |
| Lock-Free Pool | 8.9827 | 4,453,029 | Better scaling than mutex, still limited by CAS contention |
| TLS/Mutex Hybrid | 0.0211 | 1,893,107,341 | Dramatic improvement via per-thread caching |
| `new/delete` | 0.1968 | 203,220,022 | Standard allocator baseline |

> **Observation:** Thread-local caching provides the most significant gains for multi-threaded workloads. Lock-free design mitigates the mutex bottleneck but is still slower than TLS batching under high thread counts.

---

## 🔑 Key Findings

1. **Mutex-based pools** are simple and effective for single-threaded or lightly-threaded workloads but scale poorly with contention.  
2. **Lock-free pools** remove the mutex but introduce CAS overhead. They improve scalability but are still limited under extreme contention.  
3. **TLS/mutex hybrid pools** achieve the best throughput by keeping most allocations and releases local to each thread, only touching the global pool in batches.  
4. **Cache locality** matters: all slab-based designs outperform `new/delete` in predictable memory access patterns due to contiguous memory layouts.  
5. For **highly concurrent applications**, combining thread-local caching with a global fallback pool is often the optimal trade-off between speed, memory efficiency, and implementation complexity.

---

## 💻 Usage

Compile benchmark tests:

```bash
g++ -O2 -std=c++23 benchmark.cpp -o benchmark -pthread
./benchmark
```
