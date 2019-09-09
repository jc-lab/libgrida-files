/**
 * @file	limited_memory_pool.cpp
 * @author	Jichan (development@jc-lab.net / http://ablog.jc-lab.net/ )
 * @date	2019/09/04
 * @copyright Copyright (C) 2019 jichan.\n
 *            This software may be modified and distributed under the terms
 *            of the Apache License 2.0.  See the LICENSE file for details.
 */

#include "limited_memory_pool.hpp"

namespace grida {

	LimitedMemoryPool::LimitedMemoryPool(size_t max_size) : max_size_(max_size), usage_size_(0) {

	}

	std::unique_ptr<LimitedMemoryPool::PooledPack> LimitedMemoryPool::allocatePack(size_t size) {
		void* ptr = requestNativeType(size);
		if (!ptr)
			return nullptr;
		return std::unique_ptr<PooledPack>(new PooledPack(ptr, size));
	}

	void* LimitedMemoryPool::requestNativeType(size_t size) {
		std::unique_lock<std::mutex> lock(mutex_);
		auto &iter_pool = pool_[size];

		char* new_ptr;
		PooledBuffer* new_head;
		size_t new_size;

		for (auto iter = iter_pool.begin(); iter != iter_pool.end(); iter++) {
			if (!(*iter)->used_) {
				(*iter)->used_ = true;
				usage_size_ += size;
				return ((char*)iter->get()) + sizeof(PooledBuffer);
			}
		}

		new_size = usage_size_.load() + size;
		if (new_size > max_size_) {
			return NULL;
		}

		new_ptr = (char*)malloc(sizeof(PooledBuffer) + size);
		new_head = (PooledBuffer*)new_ptr;
		new (new_head) PooledBuffer(this, size);
		new_head->used_ = true;

		iter_pool.push_back(std::unique_ptr<PooledBuffer, void(*)(PooledBuffer*)>(new_head, pooledBufferDeleter));
		usage_size_ += size;

		return new_ptr + sizeof(PooledBuffer);
	}

	void LimitedMemoryPool::pooledBufferDeleter(PooledBuffer* ptr) {
		ptr->~PooledBuffer();
		free(ptr);
	}

	void LimitedMemoryPool::nativeTypeDeleter(void* ptr) {
		char* temp = (char*)ptr;
		PooledBuffer* head = (PooledBuffer*)(temp - sizeof(PooledBuffer));
		head->used_ = false;
		head->pool_->usage_size_ -= head->size_;
	}
}
