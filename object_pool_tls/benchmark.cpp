#include <iostream>
#include <chrono>
#include <thread>
#include <vector>
#include <algorithm>
#include <numeric>
#include <barrier>
#include <unistd.h>
#include <sys/ioctl.h>
#include <sys/syscall.h>
#include <linux/perf_event.h>
#include <string.h>
#include <x86intrin.h>
#include <pthread.h>
#include <iomanip>

#include "object_pool.hpp"

// --- Hardware Utilities ---

inline void keep_alive(void* p) {
    asm volatile("" : : "r,m"(p) : "memory");
}

void pin_to_core(int core_id) {
    cpu_set_t cpuset;
    CPU_ZERO(&cpuset);
    CPU_SET(core_id, &cpuset);
    pthread_setaffinity_np(pthread_self(), sizeof(cpu_set_t), &cpuset);
}

// Serialized rdtsc
inline uint64_t rdtsc() {
    unsigned int aux;
    _mm_lfence();
    uint64_t t = __rdtscp(&aux);
    _mm_lfence();
    return t;
}

uint64_t get_measurement_overhead() {
    uint64_t start = rdtsc();
    uint64_t end   = rdtsc();
    return end - start;
}

// --- Perf Event Wrapper ---

struct PerfCounter {
    int fd{-1};
    void open(uint64_t type, uint64_t config) {
        struct perf_event_attr pe;
        memset(&pe, 0, sizeof(pe));
        pe.type = type;
        pe.size = sizeof(pe);
        pe.config = config;
        pe.disabled = 1;
        pe.exclude_kernel = 1;
        pe.exclude_hv = 1;
        pe.inherit = 1; 
        fd = syscall(__NR_perf_event_open, &pe, 0, -1, -1, 0);
    }
    void reset()   { if(fd != -1) ioctl(fd, PERF_EVENT_IOC_RESET, 0); }
    void enable()  { if(fd != -1) ioctl(fd, PERF_EVENT_IOC_ENABLE, 0); }
    void disable() { if(fd != -1) ioctl(fd, PERF_EVENT_IOC_DISABLE, 0); }
    uint64_t read_val() {
        uint64_t count = 0;
        if(fd != -1) ::read(fd, &count, sizeof(count));
        return count;
    }
    ~PerfCounter() { if(fd != -1) close(fd); }
};

// --- Test Configuration ---

struct TestObject { 
    int a, b; 
    double c, d; 
    TestObject() : a(1), b(2), c(3), d(4) {} 
};

constexpr size_t OPERATIONS = 5'000'000;
constexpr size_t THREADS    = 4;
constexpr size_t POOL_SIZE  = 1'000'000;
constexpr int RUNS          = 10;

// --- Measurement Logic ---

struct Metrics {
    std::vector<uint64_t> latencies;
    uint64_t l1_misses{0};
    uint64_t l3_misses{0};
    double throughput{0};
};

void print_report(const std::string& label, const Metrics& m) {
    auto tmp = m.latencies;
    std::sort(tmp.begin(), tmp.end());
    uint64_t p50 = tmp[tmp.size() * 0.50];
    uint64_t p90 = tmp[tmp.size() * 0.90];
    uint64_t p99 = tmp[tmp.size() * 0.99];
    uint64_t p999 = tmp[tmp.size() * 0.999];
    uint64_t p9999 = tmp[tmp.size() * 0.9999];

    std::cout << std::left << std::setw(12) << label 
              << " | " << std::setw(5) << p50 
              << " | " << std::setw(5) << p90 
              << " | " << std::setw(5) << p99 
              << " | " << std::setw(5) << p999 
              << " | " << std::setw(5) << p9999 
              << " | " << std::fixed << std::setprecision(2) << std::setw(12) << m.throughput / 1e6 << " Mops/s"
              << " | L1: " << std::setw(8) << m.l1_misses 
              << " | L3: " << std::setw(6) << m.l3_misses << "\n";
}

// --- Single Threaded Phase ---

Metrics run_st_phase(ObjectPool<TestObject>& pool, bool use_pool, uint64_t overhead) {
    pin_to_core(0);
    
    std::vector<uint64_t> latencies(OPERATIONS);
    PerfCounter l1, l3;

    l1.open(PERF_TYPE_HW_CACHE,
        PERF_COUNT_HW_CACHE_L1D |
        (PERF_COUNT_HW_CACHE_OP_READ << 8) |
        (PERF_COUNT_HW_CACHE_RESULT_MISS << 16));

    l3.open(PERF_TYPE_HW_CACHE,
        PERF_COUNT_HW_CACHE_LL |
        (PERF_COUNT_HW_CACHE_OP_READ << 8) |
        (PERF_COUNT_HW_CACHE_RESULT_MISS << 16));

    // TLS warmup
    for(int i = 0; i < 1024; ++i) {
        auto* p = pool.acquire();
        pool.release(p);
    }

    auto start_wall = std::chrono::steady_clock::now();
    l1.enable(); l3.enable();

    for (size_t i = 0; i < OPERATIONS; ++i) {
        uint64_t s = rdtsc();

        if (use_pool) {
            TestObject* obj = pool.acquire();
            keep_alive(obj);
            pool.release(obj);
        } else {
            TestObject* obj = new TestObject();
            keep_alive(obj);
            delete obj;
        }

        uint64_t e = rdtsc();
        latencies[i] = e - s;
    }

    l1.disable(); l3.disable();
    auto end_wall = std::chrono::steady_clock::now();

    Metrics m;
    double seconds = std::chrono::duration<double>(end_wall - start_wall).count();
    m.throughput = OPERATIONS / seconds;
    m.l1_misses = l1.read_val();
    m.l3_misses = l3.read_val();
    m.latencies = std::move(latencies);
    return m;
}

// --- Multi Threaded Phase ---

Metrics run_mt_phase(ObjectPool<TestObject>& pool, bool use_pool, uint64_t overhead) {
    std::barrier sync(THREADS);
    std::vector<std::vector<uint64_t>> thread_latencies(THREADS, std::vector<uint64_t>(OPERATIONS));
    std::vector<uint64_t> thread_l1(THREADS), thread_l3(THREADS);
    
    auto start_wall = std::chrono::steady_clock::now();
    std::vector<std::jthread> workers;

    for (int t = 0; t < THREADS; ++t) {
        workers.emplace_back([&, t, use_pool] {
            pin_to_core(t);
            
            PerfCounter l1, l3;

            l1.open(PERF_TYPE_HW_CACHE,
                PERF_COUNT_HW_CACHE_L1D |
                (PERF_COUNT_HW_CACHE_OP_READ << 8) |
                (PERF_COUNT_HW_CACHE_RESULT_MISS << 16));

            l3.open(PERF_TYPE_HW_CACHE,
                PERF_COUNT_HW_CACHE_LL |
                (PERF_COUNT_HW_CACHE_OP_READ << 8) |
                (PERF_COUNT_HW_CACHE_RESULT_MISS << 16));

            // TLS warmup per thread
            for(int i = 0; i < 1024; ++i) {
                auto* p = pool.acquire();
                pool.release(p);
            }

            sync.arrive_and_wait();
            l1.enable(); l3.enable();

            auto& my_lats = thread_latencies[t];

            for (size_t i = 0; i < OPERATIONS; ++i) {
                uint64_t s = rdtsc();

                if (use_pool) {
                    TestObject* obj = pool.acquire();
                    keep_alive(obj);
                    pool.release(obj);
                } else {
                    TestObject* obj = new TestObject();
                    keep_alive(obj);
                    delete obj;
                }

                uint64_t e = rdtsc();
                my_lats[i] = e - s;
            }

            l1.disable(); l3.disable();
            thread_l1[t] = l1.read_val();
            thread_l3[t] = l3.read_val();
        });
    }

    workers.clear(); 
    auto end_wall = std::chrono::steady_clock::now();

    Metrics m;
    double seconds = std::chrono::duration<double>(end_wall - start_wall).count();
    m.throughput = (OPERATIONS * THREADS) / seconds;
    m.l1_misses = std::accumulate(thread_l1.begin(), thread_l1.end(), 0ULL);
    m.l3_misses = std::accumulate(thread_l3.begin(), thread_l3.end(), 0ULL);

    for (auto& v : thread_latencies)
        m.latencies.insert(m.latencies.end(), v.begin(), v.end());

    return m;
}

int main() {
    ObjectPool<TestObject> pool(POOL_SIZE);
    uint64_t overhead = get_measurement_overhead();

    std::cout << "Warming up system (frequency ramp-up)..." << std::endl;
    for(int i = 0; i < 2000000; ++i) {
        auto* p = new TestObject();
        keep_alive(p);
        delete p;
    }

    // SINGLE THREADED RUNS
    std::cout << "\n================================================================================\n";
    std::cout << "PHASE 1: SINGLE-THREADED (Pinned to Core 0)\n";
    std::cout << "================================================================================\n";
    for(int r = 0; r < RUNS; ++r) {
        std::cout << "\nRun " << r+1 << ":\n";
        std::cout << "ALLOCATOR    | p50   | p90   | p99   | p999  | p9999 | Throughput     | Hardware Counters\n";
        std::cout << "-------------|-------|-------|-------|-------|-------|----------------|------------------\n";
        print_report("NEW/DELETE", run_st_phase(pool, false, overhead));
        print_report("TLS_POOL",   run_st_phase(pool, true,  overhead));
    }

    // MULTI THREADED RUNS
    std::cout << "\n\n================================================================================\n";
    std::cout << "PHASE 2: MULTI-THREADED (" << THREADS << " Threads, Pinned)\n";
    std::cout << "================================================================================\n";
    for(int r = 0; r < RUNS; ++r) {
        std::cout << "\nRun " << r+1 << ":\n";
        std::cout << "ALLOCATOR    | p50   | p90   | p99   | p999  | p9999 | Throughput     | Hardware Counters\n";
        std::cout << "-------------|-------|-------|-------|-------|-------|----------------|------------------\n";
        print_report("NEW/DELETE", run_mt_phase(pool, false, overhead));
        print_report("TLS_POOL",   run_mt_phase(pool, true,  overhead));
    }

    return 0;
}
