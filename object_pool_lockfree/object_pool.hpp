#pragma once

#include <cstddef>
#include <new>
#include <stdexcept>
#include <atomic>

template<typename T>
union PoolNode {
	T obj;
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

			//Initialize tagged pointer (uint64_t)
			uint64_t init = pack(&pool[0], 0);

			//Store tagged pointer in free_head
			free_head.store(init, std::memory_order_relaxed);

		}

		//Object pool destructor
		~ObjectPool(){
			::operator delete(pool);
		}

		//Acquire method
		T* acquire(){

			uint64_t old_head = free_head.load(std::memory_order_acquire);

			while(true){
				Node* node = unpack_ptr(old_head);
				if(!node) return nullptr;

				uint16_t tag = unpack_tag(old_head);

				uint64_t new_head = pack(node->next, tag+1);

				if(free_head.compare_exchange_weak(
						old_head, new_head,
						std::memory_order_acq_rel,
						std::memory_order_acquire))
				{
					return new (&node->obj) T();
				}

			}
			
		}


		//Release method
		void release(T* obj){

			obj->~T();

			Node* node = reinterpret_cast<Node*>(obj);

			uint64_t old_head = free_head.load(std::memory_order_acquire);

			while(true){
				Node* head_ptr = unpack_ptr(old_head);
				uint16_t tag = unpack_tag(old_head);

				node->next = head_ptr;

				uint64_t new_head = pack(node, tag+1);

				if(free_head.compare_exchange_weak(
							old_head, new_head,
							std::memory_order_acq_rel,
							std::memory_order_acquire))
				{
					return;
				}
			}
		}

	private:
		using Node = PoolNode<T>;

		Node* pool;
		std::atomic<uint64_t> free_head;
		size_t capacity;

		static constexpr uint64_t PTR_MASK = (1ULL<<48)-1;
		static constexpr uint64_t TAG_SHIFT = 48;

		static uint64_t pack(Node* ptr, uint16_t tag){
			uint64_t p = reinterpret_cast<uint64_t>(ptr);
			return (static_cast<uint64_t>(tag) << TAG_SHIFT) | (p & PTR_MASK);
		}

		static Node* unpack_ptr(uint64_t value){
			return reinterpret_cast<Node*>(value & PTR_MASK);
		}

		static uint16_t unpack_tag(uint64_t value){
			return static_cast<uint16_t>(value >> TAG_SHIFT);
		}

};
