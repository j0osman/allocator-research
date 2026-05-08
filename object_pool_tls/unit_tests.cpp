#include <cassert>
#include <iostream>
#include <vector>
#include <thread>
#include <atomic>

#include "object_pool.hpp"

//
// ---------- Test Object
//
struct TestObject {
    static std::atomic<int> ctor;
    static std::atomic<int> dtor;

    int value;

    TestObject() : value(42) { ctor++; }
    ~TestObject() { dtor++; }
};

std::atomic<int> TestObject::ctor{0};
std::atomic<int> TestObject::dtor{0};

void reset_counters() {
    TestObject::ctor = 0;
    TestObject::dtor = 0;
}

//
// ---------- Basic allocate/free
//
void test_basic_allocate() {
    reset_counters();

    ObjectPool<TestObject> pool(16);

    TestObject* obj = pool.acquire();
    assert(obj);
    assert(obj->value == 42);

    pool.release(obj);

    assert(TestObject::ctor == 1);
    assert(TestObject::dtor == 1);

    std::cout << "✓ basic allocate\n";
}

//
// ---------- Capacity exhaustion
//
void test_capacity() {
    reset_counters();

    ObjectPool<TestObject> pool(4);

    TestObject* a = pool.acquire();
    TestObject* b = pool.acquire();
    TestObject* c = pool.acquire();
    TestObject* d = pool.acquire();
    TestObject* e = pool.acquire();

    assert(a && b && c && d);
    assert(e == nullptr);

    pool.release(a);
    pool.release(b);
    pool.release(c);
    pool.release(d);

    assert(TestObject::ctor == 4);
    assert(TestObject::dtor == 4);

    std::cout << "✓ capacity\n";
}

//
// ---------- Reuse freed nodes
//
void test_reuse() {
    reset_counters();

    ObjectPool<TestObject> pool(2);

    TestObject* a = pool.acquire();
    pool.release(a);

    TestObject* b = pool.acquire();

    assert(a == b);

    pool.release(b);

    assert(TestObject::ctor == 2);
    assert(TestObject::dtor == 2);

    std::cout << "✓ reuse\n";
}

//
// ---------- Bulk allocate
//
void test_bulk() {
    reset_counters();

    constexpr int N = 256;
    ObjectPool<TestObject> pool(N);

    std::vector<TestObject*> vec;
    vec.reserve(N);

    for(int i=0;i<N;i++){
        auto* p = pool.acquire();
        assert(p);
        vec.push_back(p);
    }

    for(auto* p : vec)
        pool.release(p);

    assert(TestObject::ctor == N);
    assert(TestObject::dtor == N);

    std::cout << "✓ bulk allocate\n";
}

//
// ---------- Multi-thread stress
//
void worker(ObjectPool<TestObject>* pool, int iters) {
    for(int i=0;i<iters;i++){
        auto* obj = pool->acquire();
        assert(obj);
        pool->release(obj);
    }
}

void test_multithread() {
    reset_counters();

    ObjectPool<TestObject> pool(1024);

    constexpr int THREADS = 4;
    constexpr int ITERS   = 100000;

    std::vector<std::thread> threads;

    for(int i=0;i<THREADS;i++)
        threads.emplace_back(worker, &pool, ITERS);

    for(auto& t : threads)
        t.join();

    assert(TestObject::ctor == TestObject::dtor);

    std::cout << "✓ multithread\n";
}

//
// ---------- Alignment test
//
struct alignas(64) BigAlign {
    char data[64];
};

void test_alignment() {
    ObjectPool<BigAlign> pool(8);

    auto* p = pool.acquire();

    assert(reinterpret_cast<uintptr_t>(p) % 64 == 0);

    pool.release(p);

    std::cout << "✓ alignment\n";
}

//
// ---------- Lifecycle symmetry
//
void test_lifecycle() {
    reset_counters();

    {
        ObjectPool<TestObject> pool(32);

        std::vector<TestObject*> vec;

        for(int i=0;i<32;i++)
            vec.push_back(pool.acquire());

        for(auto* p : vec)
            pool.release(p);
    }

    assert(TestObject::ctor == TestObject::dtor);

    std::cout << "✓ lifecycle\n";
}

//
// ---------- Cross Thread Free
//
void test_cross_thread_free() {
    reset_counters();

    ObjectPool<TestObject> pool(128);

    std::vector<TestObject*> objs;

    for(int i=0;i<64;i++)
        objs.push_back(pool.acquire());

    std::thread t([&]{
        for(auto* p : objs)
            pool.release(p);
    });

    t.join();

    assert(TestObject::ctor == 64);
    assert(TestObject::dtor == 64);

    std::cout << "✓ cross-thread free\n";
}

//
// ---------- Multiple pools in same thread
//
void test_multiple_pools_same_thread() {
    reset_counters();

    ObjectPool<TestObject> pool1(16);
    ObjectPool<TestObject> pool2(16);

    auto* a = pool1.acquire();
    pool1.release(a);

    auto* b = pool2.acquire();
    pool2.release(b);

    auto* c = pool1.acquire();
    pool1.release(c);

    assert(TestObject::ctor == 3);
    assert(TestObject::dtor == 3);

    std::cout << "✓ multiple pools same thread\n";
}

//
// ---------- Multithreaded exhaustion
//
void test_multithread_exhaustion() {
    reset_counters();

    ObjectPool<TestObject> pool(32);

    constexpr int THREADS = 8;

    std::vector<std::thread> threads;

    for(int t=0;t<THREADS;t++){
        threads.emplace_back([&]{
            std::vector<TestObject*> local;

            for(int i=0;i<8;i++){
                if(auto* p = pool.acquire())
                    local.push_back(p);
            }

            for(auto* p : local)
                pool.release(p);
        });
    }

    for(auto& t : threads)
        t.join();

    assert(TestObject::ctor == TestObject::dtor);

    std::cout << "✓ multithread exhaustion\n";
}

//
// ---------- Zero-sized type
//
struct Empty {};

void test_empty_type() {
    ObjectPool<Empty> pool(8);

    auto* p = pool.acquire();
    assert(p);

    pool.release(p);

    std::cout << "✓ empty type\n";
}

//
// ---------- MAIN
//
int main() {
    std::cout << "==== ObjectPool Unit Tests ====\n";

    test_basic_allocate();
    test_capacity();
    test_reuse();
    test_bulk();
    test_multithread();
    test_alignment();
    test_lifecycle();
	test_cross_thread_free();
	test_multiple_pools_same_thread();
	test_multithread_exhaustion();
	test_empty_type();

    std::cout << "All tests passed.\n";
}
