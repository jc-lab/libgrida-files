/**
 * @file	thread_pool.hpp
 * @author	Jichan (development@jc-lab.net / http://ablog.jc-lab.net/ )
 * @date	2019/09/04
 * @copyright Copyright (C) 2019 jichan.\n
 *            This software may be modified and distributed under the terms
 *            of the Apache License 2.0.  See the LICENSE file for details.
 */

#ifndef __GRIDA_THREAD_POOL_HPP__
#define __GRIDA_THREAD_POOL_HPP__

#include <memory>
#include <thread>
#include <list>
#include <tuple>

#include <deque>
#include <atomic>
#include <mutex>
#include <condition_variable>

namespace grida {

	class ThreadPool {
	public:
		struct QueueItem {
			virtual void run() = 0;
		};

	private:
		template<typename F, typename Tuple>
		class Binder : public QueueItem
		{
		protected:
			F f_;
			Tuple tuple_;

		public:
			Binder(F f, Tuple&& tuple) : f_(f), tuple_(std::move(tuple)) {
			}

			template<typename F, typename Tuple, size_t ...S >
			void run_impl(F&& fn, Tuple&& t, std::index_sequence<S...>)
			{
				return std::forward<F>(fn)(std::get<S>(std::forward<Tuple>(t))...);
			}

			void run() override {
				std::size_t constexpr tSize
					= std::tuple_size<typename std::remove_reference<Tuple>::type>::value;

				run_impl(std::forward<F>(f_),
					std::forward<Tuple>(tuple_),
					std::make_index_sequence<tSize>());
			}
		};

		struct {
			std::mutex mutex;
			std::condition_variable cond;
			std::deque<std::unique_ptr<QueueItem>> queue;
		} queue_;

		std::list<std::unique_ptr<std::thread>> threads_;

		std::atomic_bool thread_run_;

		void worker();

	public:
		ThreadPool();
		~ThreadPool();

		void start(int num_of_threads);
		void stop();

		template<class F, class... Args>
		void send(F&& f, Args... args) {
			{
				std::unique_lock<std::mutex> lock(queue_.mutex);
				queue_.queue.push_back(std::make_unique<Binder<F, std::tuple<Args...>>>(f, std::make_tuple(std::forward<Args>(args)...)));
			}
			queue_.cond.notify_one();
		}

		template<class F, class... Args>
		static std::unique_ptr<QueueItem> taskUniqueBind(F&& f, Args... args) {
			return std::make_unique<Binder<F, std::tuple<Args...>>>(f, std::make_tuple(std::forward<Args>(args)...));
		}

		template<class F, class... Args>
		static std::shared_ptr<QueueItem> taskSharedBind(F&& f, Args... args) {
			return std::make_shared<Binder<F, std::tuple<Args...>>>(f, std::make_tuple(std::forward<Args>(args)...));
		}
	};

} // namespace grida

#endif // __GRIDA_THREAD_POOL_HPP__
