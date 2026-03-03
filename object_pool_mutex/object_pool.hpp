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
			free_head = &pool[0];
		}

		//Object pool destructor
		~ObjectPool(){
			::operator delete(pool);
		}

		//Acquire method
		T* acquire(){

			//lock_guard for smart mutex unlocking and destruction
			std::lock_guard<std::mutex> lock(mtx);

			if(!free_head){
				return nullptr;
			}

			//redirect free_head to next node
			Node* node = free_head;
			free_head = node->next;

			//construct T inside union object memory
			return new (&node->object) T();
		}


		//Release method
		void release(T* obj){

			//lock_guard for smart mutex unlocking and destruction
			std::lock_guard<std::mutex> lock(mtx);

			//destroy node object
			obj->~T();

			//change union object back to pointer
			Node* node = reinterpret_cast<Node*>(obj);

			//reinsert node in freelist and point to free_head
			node->next = free_head;
			free_head = node;
		}

	private:
		using Node = PoolNode<T>;

		Node* pool;
		Node* free_head;
		size_t capacity;

		std::mutex mtx;

};
