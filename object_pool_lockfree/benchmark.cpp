#include <iostream>
#include <chrono>
#include <thread>
#include <vector>
#include <print>

#include "object_pool.hpp"

struct TestObject {
	int a, b;
	double c, d;

	TestObject() : a(1), b(2), c(3.0), d(4.0) {}
};

constexpr size_t OPERATIONS = 5'000'000;
constexpr size_t THREADS = 8;
constexpr size_t POOL_SIZE = 1'000'000;

using Clock = std::chrono::high_resolution_clock;

//Duration measuring function
template<typename F>
double measure(F&& func){
	auto start = Clock::now();
	func();
	auto end = Clock::now();

	std::chrono::duration<double> diff = end-start;
	return diff.count();
}

//Single threaded new/delete benchmark
double benchmark_new_delete(){
	return measure([]{
		volatile int sink = 0;

		for(size_t i=0; i<OPERATIONS; i++){
			TestObject* obj = new TestObject();
			sink+=obj->a;
			delete obj;
		}
	});
}

//Single threaded pool benchmark
double benchmark_pool(ObjectPool<TestObject>& pool){
	return measure([&]{
		volatile int sink = 0;

		for(size_t i=0; i<OPERATIONS; i++){
			TestObject* obj = pool.acquire();
			sink+=obj->a;
			if(obj)
				pool.release(obj);
		}
	});
}

//Multithreaded new/delete benchmark
double benchmark_new_delete_mt(){
    return measure([]{

        std::vector<std::thread> threads;

        for (size_t t = 0; t<THREADS; t++){
            threads.emplace_back([]{

                volatile int sink = 0;

                for (size_t i = 0; i < OPERATIONS; i++)
                {
                    TestObject* obj = new TestObject();
                    sink += obj->a;
                    delete obj;
                }

            });
        }

        for (auto& t : threads)
            t.join();

    });
}

//Multithreaded pool benchmark
double benchmark_pool_mt(ObjectPool<TestObject>& pool){
    return measure([&]{

        std::vector<std::thread> threads;

        for (size_t t = 0; t < THREADS; t++){
            threads.emplace_back([&]{

                volatile int sink = 0;

                for (size_t i = 0; i < OPERATIONS; i++)
                {
                    TestObject* obj = pool.acquire();
                    sink += obj->a;
                    pool.release(obj);
                }

            });
        }

        for (auto& t : threads)
            t.join();

    });
}


int main(){
	ObjectPool<TestObject> pool(POOL_SIZE);

	std::println("Single Thread");

	double new_time = benchmark_new_delete();
	double pool_time = benchmark_pool(pool);

	std::println("new/delete : {} sec", new_time);
	std::println("pool       : {} sec\n", pool_time);

	std::println("Multi Thread ({} threads)", THREADS);

	new_time = benchmark_new_delete_mt();
	pool_time = benchmark_pool_mt(pool);

	std::println("new/delete : {} sec", new_time);
	std::println("pool       : {} sec", pool_time);
}

