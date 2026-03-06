#include <iostream>
#include <chrono>
#include <thread>
#include <vector>
#include <barrier>
#include <print>

#include "object_pool.hpp"

struct TestObject {
    int a, b;
    double c, d;

    TestObject() : a(1), b(2), c(3.0), d(4.0) {}
};

constexpr size_t OPERATIONS = 5'000'000;
constexpr size_t THREADS    = 8;
constexpr size_t POOL_SIZE  = 1'000'000;
constexpr int RUNS          = 5;

using Clock = std::chrono::steady_clock;

template<typename F>
double measure(F&& func)
{
    auto start = Clock::now();
    func();
    auto end = Clock::now();

    std::chrono::duration<double> diff = end - start;
    return diff.count();
}

double average(std::vector<double>& v)
{
    double sum = 0;
    for (auto x : v) sum += x;
    return sum / v.size();
}

//////////////////////////////////////////////////////////
// Single-thread new/delete
//////////////////////////////////////////////////////////

double benchmark_new_delete()
{
    volatile int sink = 0;

    return measure([&]{
        for(size_t i = 0; i < OPERATIONS; i++)
        {
            TestObject* obj = new TestObject();
            sink += obj->a;
            delete obj;
        }
    });
}

//////////////////////////////////////////////////////////
// Single-thread pool
//////////////////////////////////////////////////////////

double benchmark_pool(ObjectPool<TestObject>& pool)
{
    volatile int sink = 0;

    return measure([&]{
        for(size_t i = 0; i < OPERATIONS; i++)
        {
            TestObject* obj = pool.acquire();
            if(obj){
                sink += obj->a;
                pool.release(obj);
            }
        }
    });
}

//////////////////////////////////////////////////////////
// Multithreaded benchmark helper
//////////////////////////////////////////////////////////

template<typename AllocFunc>
double benchmark_mt(AllocFunc func)
{
    std::barrier sync(THREADS);

    return measure([&]{

        std::vector<std::thread> threads;
        threads.reserve(THREADS);

        for(size_t t = 0; t < THREADS; t++)
        {
            threads.emplace_back([&]{

                volatile int sink = 0;

                sync.arrive_and_wait();

                for(size_t i = 0; i < OPERATIONS; i++)
                {
                    func(sink);
                }

            });
        }

        for(auto& t : threads)
            t.join();
    });
}

//////////////////////////////////////////////////////////
// MT new/delete
//////////////////////////////////////////////////////////

double benchmark_new_delete_mt()
{
    return benchmark_mt([](volatile int& sink){

        TestObject* obj = new TestObject();
        sink += obj->a;
        delete obj;

    });
}

//////////////////////////////////////////////////////////
// MT pool
//////////////////////////////////////////////////////////

double benchmark_pool_mt(ObjectPool<TestObject>& pool)
{
    return benchmark_mt([&](volatile int& sink){

        TestObject* obj = pool.acquire();
        if(obj){
            sink += obj->a;
            pool.release(obj);
        }

    });
}

//////////////////////////////////////////////////////////

void run_single(ObjectPool<TestObject>& pool)
{
    std::vector<double> new_times;
    std::vector<double> pool_times;

    for(int i = 0; i < RUNS; i++)
    {
        new_times.push_back(benchmark_new_delete());
        pool_times.push_back(benchmark_pool(pool));
    }

    double new_avg  = average(new_times);
    double pool_avg = average(pool_times);

    std::println("Single Thread");
    std::println("new/delete : {:.6f} sec", new_avg);
    std::println("pool       : {:.6f} sec", pool_avg);

    double new_ops  = OPERATIONS / new_avg;
    double pool_ops = OPERATIONS / pool_avg;

    std::println("throughput new/delete : {:.0f} ops/sec", new_ops);
    std::println("throughput pool       : {:.0f} ops/sec\n", pool_ops);
}

void run_multi(ObjectPool<TestObject>& pool)
{
    std::vector<double> new_times;
    std::vector<double> pool_times;

    for(int i = 0; i < RUNS; i++)
    {
        new_times.push_back(benchmark_new_delete_mt());
        pool_times.push_back(benchmark_pool_mt(pool));
    }

    double new_avg  = average(new_times);
    double pool_avg = average(pool_times);

    std::println("Multi Thread ({} threads)", THREADS);
    std::println("new/delete : {:.6f} sec", new_avg);
    std::println("pool       : {:.6f} sec", pool_avg);

    double total_ops = OPERATIONS * THREADS;

    double new_ops  = total_ops / new_avg;
    double pool_ops = total_ops / pool_avg;

    std::println("throughput new/delete : {:.0f} ops/sec", new_ops);
    std::println("throughput pool       : {:.0f} ops/sec", pool_ops);
}

//////////////////////////////////////////////////////////

int main()
{
    ObjectPool<TestObject> pool(POOL_SIZE);

    // warmup
    benchmark_new_delete();
    benchmark_pool(pool);

    run_single(pool);
    run_multi(pool);
}
