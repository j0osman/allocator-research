#pragma once

#include <cstddef>
#include <stdexcept>
#include <sys/mman.h>
#include <new>
#include <atomic>

template<typename T>
struct alignas(64) PoolNode {
	union {
		T object;
		PoolNode* next;
	};
};

template<typename T>
class ObjectPool {
	using Node = PoolNode<T>;
public:
	explicit ObjectPool(size_t capacity) : capacity(capacity){
		if(capacity==0) throw std::invalid_argument("Capacity must be > 0");

		size_t total_size = sizeof(Node)*capacity;
		pool = static_cast<Node*>(mmap(nullptr, total_size,
									   PROT_READ | PROT_WRITE,
									   MAP_PRIVATE | MAP_ANONYMOUS | MAP_HUGETLB,
									   -1, 0));

		if(pool==MAP_FAILED){
			using_mmap = false;
			pool = static_cast<Node*>(::operator new(total_size, std::align_val_t{alignof(Node)}));
		}else using_mmap = true;

		for(size_t i=0; i<capacity-1; i++){
			pool[i].next = &pool[i+1];
		}
		pool[capacity-1].next = nullptr;
		global_head.store(&pool[0], std::memory_order_relaxed);
	}

	~ObjectPool(){
		if(tls_owner == this){
			tls_head = nullptr;
			tls_count = 0;
			tls_owner = nullptr;
		}

		if(using_mmap) munmap(pool, sizeof(Node)*capacity);
		else ::operator delete(pool, std::align_val_t{alignof(Node)});
	}

	T* acquire(){
		if(tls_owner && tls_owner != this){
			tls_owner->flush_foreign_tls(tls_head);
			tls_head = nullptr;
			tls_count = 0;
		}
		tls_owner = this;

		if(__builtin_expect(tls_head!=nullptr, 1)){
			Node* node = tls_head;
			tls_head = node->next;
			tls_count--;

			return new (&node->object) T();
		}

		refill_tls();
		if(!tls_head) return nullptr;

		Node* node = tls_head;
		tls_head = node->next;
		tls_count--;

		return new (&node->object) T();
	}

	void release(T* obj){
		obj->~T();

		if(tls_owner && tls_owner != this){
			tls_owner->flush_foreign_tls(tls_head);
			tls_head = nullptr;
			tls_count = 0;
		}
		tls_owner = this;

		Node* node = reinterpret_cast<Node*>(obj);
		node->next = tls_head;
		tls_head = node;
		tls_count++;

		if(__builtin_expect(tls_count > TLS_LIMIT, 0)){
			flush_tls();
		}
	}

private:
	size_t capacity;

	Node* pool;
	alignas(64) std::atomic<Node*> global_head;

	bool using_mmap;

	static thread_local Node* tls_head;
	static thread_local size_t tls_count;
	static thread_local ObjectPool* tls_owner;

	static constexpr size_t TLS_BATCH = 128;
	static constexpr size_t TLS_LIMIT = 512;

	void refill_tls(){
		while(true){
			Node* head = global_head.load(std::memory_order_relaxed);
			if(!head) return;

			Node* batch_head = head;
			Node* batch_tail = head;

			size_t count = 1;
			while(batch_tail->next && count < TLS_BATCH){
				batch_tail = batch_tail->next;
				count++;
			}

			Node* new_head = batch_tail->next;

			if(global_head.compare_exchange_weak(
						head,
						new_head,
						std::memory_order_acq_rel,
						std::memory_order_acquire)){
				batch_tail->next = tls_head;
				tls_head = batch_head;
				tls_count+=count;
				return;
			}
		}
	}

	void flush_tls(){
		if(!tls_head) return;

		Node* batch_head = tls_head;
		Node* batch_tail = tls_head;

		size_t count = 1;
		while(batch_tail->next && count<TLS_BATCH){
			batch_tail = batch_tail->next;
			count++;
		}

		tls_head = batch_tail->next;
		tls_count-=count;

		while(true){
			Node* head = global_head.load(std::memory_order_acquire);

			batch_tail->next = head;

			if(global_head.compare_exchange_weak(
						head,
						batch_head,
						std::memory_order_acq_rel,
						std::memory_order_acquire)){
				return;
			}
		}
	}

	void flush_foreign_tls(Node* head){
		if(!head) return;

		Node* tail = head;
		while(tail->next) tail = tail->next;

		while(true){
			Node* g = global_head.load(std::memory_order_acquire);
			tail->next = g;

			if(global_head.compare_exchange_weak(
						g,
						head,
						std::memory_order_acq_rel,
						std::memory_order_acquire))
				return;
		}
	}
};

template<typename T> 
thread_local typename ObjectPool<T>::Node* ObjectPool<T>::tls_head = nullptr;

template<typename T> 
thread_local size_t ObjectPool<T>::tls_count = 0;

template<typename T>
thread_local ObjectPool<T>* ObjectPool<T>::tls_owner = nullptr;
