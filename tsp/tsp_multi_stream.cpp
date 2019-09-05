/**
 * @file	tsp_multi_stream.cpp
 * @author	Jichan (development@jc-lab.net / http://ablog.jc-lab.net/ )
 * @date	2019/09/04
 * @copyright Copyright (C) 2019 jichan.\n
 *            This software may be modified and distributed under the terms
 *            of the Apache License 2.0.  See the LICENSE file for details.
 */

#include "tsp_multi_stream.hpp"

namespace grida {

	namespace tsp {

		TspMultiStream::TspMultiStream()
			: recv_ctx_map_(this)
		{
			recv_context_factory_ = [](const std::string& remote_ip, int remote_port) { return std::unique_ptr<TspRecvContext>(new TspRecvContext()); };
		}

		TspMultiStream::~TspMultiStream()
		{
		}

		void TspMultiStream::setRecvContextFactory(RecvContextFactory factory)
		{
			recv_context_factory_ = factory;
		}

		int TspMultiStream::start(ThreadPool* thread_pool) {
			thread_pool_ = thread_pool;
			return 1;
		}

		int TspMultiStream::onRecvPacket(const std::string& remote_ip, int remote_port, std::unique_ptr<const char[]> packet_data, int packet_len) {
			if (thread_pool_) {
				thread_pool_->send([this, remote_ip, remote_port, packet_len](std::unique_ptr<const char[]> packet_data) {
					std::unique_ptr<const char[]> copied_packet_data(std::move(packet_data));
					processRecvPacket(remote_ip, remote_port, copied_packet_data, packet_len);
					}, std::move(packet_data));
				return 1;
			}
			else {
				return processRecvPacket(remote_ip, remote_port, packet_data, packet_len);
			}
		}

		int TspMultiStream::processRecvPacket(const std::string& remote_ip, int remote_port, std::unique_ptr<const char[]>& packet_data, int packet_len)
		{
			TspRecvContext* recv_ctx = recv_ctx_map_.refer(remote_ip, remote_port);
			recv_ctx->buffer_.insert(recv_ctx->buffer_.end(), packet_data.get(), packet_data.get() + packet_len);
			return processRecvPacketImpl(recv_ctx);
		}

		TspRecvContext* TspMultiStream::RecvContextMap::refer(const std::string& remote_ip, int remote_port)
		{
			TspRecvContext* ctx = NULL;
			peerkey_t key(remote_ip, remote_port);
			auto found_iter = map.find(key);
			if (found_iter != map.end()) {
				ctx = found_iter->second.ctx.get();
				lru_list.erase(found_iter->second.iter);
				lru_list.push_front(key);
				found_iter->second.iter = lru_list.begin();
			} else {
				if (map.size() > 1024) {
					remove_old();
				}
				std::unique_ptr<TspRecvContext> new_ctx(pthis_->recv_context_factory_(remote_ip, remote_port));
				ctx = new_ctx.get();
				lru_list.push_front(key);
				map.emplace(key, MapItem(lru_list.begin(), new_ctx));
			}
			return ctx;
		}

		void TspMultiStream::RecvContextMap::remove_old() {
			int index = 0;
			for (auto iter = lru_list.begin(); iter != lru_list.end(); ) {
				auto map_iter = map.find(*iter);

				if ((index >= max_bucket) || (map_iter->second.ctx->errored_)) {
					printf("Erase old element, %s:%d, %d\n", iter->first.c_str(), iter->second, map_iter->second.ctx->errored_);
					map.erase(map_iter);
					iter = lru_list.erase(iter);
				} else {
					iter++;
					index++;
				}
			}
		}

#if 0
		template<typename ...Args>
		int TspMultiStream::RecvQueue::push(Args& ...args) {
			std::lock_guard<std::mutex> lock(this->m);
			this->q.emplace(args...);
			this->c.notify_one();
			return 1;
		}
		bool TspMultiStream::RecvQueue::pop(QueueItem& out, int timeout_ms) {
			bool is_empty;
			// synchronized block
			std::unique_lock<std::mutex> lock(this->m);
			is_empty = this->q.empty();
			if (is_empty) {
				if (this->c.wait_for(lock, std::chrono::milliseconds(timeout_ms)) == std::cv_status::no_timeout)
					is_empty = this->q.empty();
			}
			if (!is_empty) {
				out = std::move(this->q.front());
				this->q.pop();
				return true;
			}
			return false;
		}

		void TspMultiStream::workerThreadProc() {
			int task_count_1 = 0;
			while (worker_run_.load()) {
				QueueItem item;
				if (recv_queue_.pop(item)) {
					processRecvPacket(item.remote_ip, item.remote_port, item.data, item.length);
				}

				task_count_1++;
				if (task_count_1 >= 10) {
					task_count_1 = 0;
					recv_ctx_map_.remove_old();
				}
			}
		}
#endif

	} // namespace tsp

} // namespace grida
