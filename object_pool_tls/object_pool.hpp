#pragma once

#include <cstddef>
#include <mutex>
#include <new>
#include <stdexcept>

template<typename T>
union PoolNode {
	T object;
	PoolNode* next;
};

template<typename T>
class ObjectPool{
	public:

		//Constructor with FreeList allocation
		explicit ObjectPool(size_t capacity)
			: capacity(capacity)
		{
			if(capacity==0)
				throw std::invalid_argument("ObjectPool capacity must be > 0");

			//get block of memory (raw bytes) from heap
			pool = static_cast<Node*>(::operator new(sizeof(Node)*capacity));

			//point each node in pool to next node
			for(size_t i=0; i<capacity-1; i++){
				pool[i].next = &pool[i+1];
			}

			//last node gets nullptr
			pool[capacity-1].next = nullptr;

			//first node gets attached to free_head, ready for allocation
			global_head = &pool[0];
		}

		//Object pool destructor
		~ObjectPool(){
			::operator delete(pool);
		}

		//Acquire method
		T* acquire(){

			if(tls_free_head){
				Node* node = tls_free_head;
				tls_free_head = node->next;
				tls_count--;
				return new (&node->object) T();
			}

			refill_tls();

			if(!tls_free_head) return nullptr;

			//redirect global_head to next node
			Node* node = tls_free_head;
			tls_free_head = node->next;
			tls_count--;

			//construct T inside union object memory
			return new (&node->object) T();
		}


		//Release method
		void release(T* obj){

			//destroy node object
			obj->~T();

			//change union object back to pointer
			Node* node = reinterpret_cast<Node*>(obj);

			//reinsert node in freelist and point to free_head
			node->next = tls_free_head;
			tls_free_head = node;

			tls_count++;

			if(tls_count>TLS_LIMIT) flush_tls();

		}

	private:
		using Node = PoolNode<T>;

		Node* pool;
		Node* global_head;
		size_t capacity;

		std::mutex global_mtx;

		static thread_local Node* tls_free_head;
		static thread_local size_t tls_count;

		static constexpr size_t TLS_BATCH = 32;
		static constexpr size_t TLS_LIMIT = 128;

		void refill_tls(){
			size_t count = 0;

			std::lock_guard<std::mutex> lock(global_mtx);

			while(global_head && count < TLS_BATCH){
				Node* node = global_head;
				global_head = node->next;

				node->next = tls_free_head;
				tls_free_head = node;

				tls_count++;
				count++;
			}
		}

		void flush_tls(){
			size_t count = 0;

			if(!tls_free_head) return;

			std::lock_guard<std::mutex> lock(global_mtx);

			while(tls_free_head && count < TLS_BATCH){
				Node* node = tls_free_head;
				tls_free_head = node->next;

				node->next = global_head;
				global_head = node;

				tls_count--;
				count++;

			}
		}

};

template<typename T>
thread_local ObjectPool<T>::Node* ObjectPool<T>::tls_free_head = nullptr;

template<typename T>
thread_local size_t ObjectPool<T>::tls_count = 0;
