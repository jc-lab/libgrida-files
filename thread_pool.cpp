/**
 * @file	thread_pool.cpp
 * @author	Jichan (development@jc-lab.net / http://ablog.jc-lab.net/ )
 * @date	2019/09/04
 * @copyright Copyright (C) 2019 jichan.\n
 *            This software may be modified and distributed under the terms
 *            of the Apache License 2.0.  See the LICENSE file for details.
 */

#include "thread_pool.hpp"

namespace grida {

	ThreadPool::ThreadPool() : thread_run_(false) {
	}

	ThreadPool::~ThreadPool() {
		stop();
	}

	void ThreadPool::start(int num_of_threads) {
		thread_run_ = true;
		for (int i = 0; i < num_of_threads; i++) {
			threads_.push_back(std::unique_ptr<std::thread>(new std::thread(std::bind(&ThreadPool::worker, this))));
		}
	}

	void ThreadPool::stop() {
		thread_run_ = false;
		for (auto iter = threads_.begin(); iter != threads_.end(); ) {
			if ((*iter)->joinable()) {
				(*iter)->join();
			}
			iter = threads_.erase(iter);
		}
	}

	void ThreadPool::worker() {
		while (thread_run_) {
			std::unique_ptr<QueueItem> item;
			do {
				std::unique_lock<std::mutex> lock(queue_.mutex);
				bool is_empty = queue_.queue.empty();
				if (is_empty) {
					if (queue_.cond.wait_for(lock, std::chrono::milliseconds(100)) == std::cv_status::timeout) {
						break;
					}
					is_empty = queue_.queue.empty();
				}
				if (!is_empty) {
					item = std::move(queue_.queue.front());
					queue_.queue.pop_front();
				}
			} while (false);
			if (item) {
				item->run();
			}
		}
	}
}
