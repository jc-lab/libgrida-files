/**
 * @file	limited_memory_pool.hpp
 * @author	Jichan (development@jc-lab.net / http://ablog.jc-lab.net/ )
 * @date	2019/09/04
 * @copyright Copyright (C) 2019 jichan.\n
 *            This software may be modified and distributed under the terms
 *            of the Apache License 2.0.  See the LICENSE file for details.
 */

#ifndef __GRIDA_LIMITED_MEMORY_POOL_HPP__
#define __GRIDA_LIMITED_MEMORY_POOL_HPP__

#include <memory>
#include <list>
#include <map>
#include <mutex>
#include <condition_variable>
#include <atomic>

namespace grida {

	class LimitedMemoryPool {
	public:
		struct PooledBuffer {
			LimitedMemoryPool* pool_;
			size_t size_;
			std::atomic_bool used_;

			PooledBuffer(LimitedMemoryPool* pool, size_t size) : pool_(pool), size_(size), used_(false) {}
		};

		class PooledPack {
		private:
			friend class LimitedMemoryPool;

			void* ptr_;
			size_t size_;
			PooledPack(void * ptr, size_t size) : ptr_(ptr), size_(size) {}

		public:
			~PooledPack() {
				nativeTypeDeleter(ptr_);
			}

			void* raw() {
				return ptr_;
			}
			size_t size() const {
				return size_;
			}
		};

		LimitedMemoryPool(size_t max_size);
		template<typename T>
		std::unique_ptr<T[], void(void*)> allocate(size_t count) {
			return std::unique_ptr<T[], void(void*)>((T[])requestNativeType(sizeof(T) * count));
		}
		std::unique_ptr<PooledPack> allocatePack(size_t size);
		static void nativeTypeDeleter(void* ptr);

	private:
		size_t max_size_;
		std::atomic_int64_t usage_size_;

		std::mutex mutex_;
		std::map<size_t, std::list<std::unique_ptr<PooledBuffer, void(*)(PooledBuffer*)>>> pool_;

		void* requestNativeType(size_t size);
		static void pooledBufferDeleter(PooledBuffer* ptr);
	};

} // namespace grida

#endif // __GRIDA_LIMITED_MEMORY_POOL_HPP__
