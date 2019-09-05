/**
 * @file	tsp_multi_stream.hpp
 * @author	Jichan (development@jc-lab.net / http://ablog.jc-lab.net/ )
 * @date	2019/09/04
 * @copyright Copyright (C) 2019 jichan.\n
 *            This software may be modified and distributed under the terms
 *            of the Apache License 2.0.  See the LICENSE file for details.
 */

#ifndef __GRIDA_TSP_TSP_MULTI_STREAM_HPP__
#define __GRIDA_TSP_TSP_MULTI_STREAM_HPP__

#include "tsp_stream.hpp"

#include "../thread_pool.hpp"

namespace grida {
	
	namespace tsp {

		class TspMultiStream : public TspStream {
		public:
			typedef std::function<std::unique_ptr<TspRecvContext>(const std::string& remote_ip, int remote_port)> RecvContextFactory;

		private:
			struct QueueItem {
				std::string remote_ip;
				int remote_port;
				std::unique_ptr<const char[]> data;
				int length;

				QueueItem() {}
				QueueItem(const std::string& in_remote_ip, int in_remote_port, std::unique_ptr<const char[]>& in_data, int in_length)
					: remote_ip(in_remote_ip), remote_port(in_remote_port), data(std::move(in_data)), length(in_length)
				{
				}
			};

#if 0
			struct RecvQueue {
				std::queue<QueueItem> q;
				std::mutex m;
				std::condition_variable c;

				template<typename ...Args>
				int push(Args& ...args);
				bool pop(QueueItem& out, int timeout_ms = 100);
			} recv_queue_;
#endif

			struct RecvContextMap {
				typedef std::pair<std::string, int> peerkey_t;
				typedef std::list< peerkey_t> lru_list_t;

				struct MapItem {
					lru_list_t::iterator iter;
					std::unique_ptr<TspRecvContext> ctx;

					MapItem(lru_list_t::iterator in_iter, std::unique_ptr<TspRecvContext>& in_ctx)
						: iter(in_iter), ctx(std::move(in_ctx)) { }
				};

				typedef std::map< peerkey_t, MapItem> map_t;

				TspMultiStream* pthis_;

				int max_bucket;
				map_t map;
				lru_list_t lru_list;

				TspRecvContext* refer(const std::string& remote_ip, int remote_port);
				void remove_old();

				RecvContextMap(TspMultiStream* pthis) : pthis_(pthis) {
					max_bucket = 1024;
				}
			} recv_ctx_map_;

			RecvContextFactory recv_context_factory_;

			ThreadPool* thread_pool_;

		public:
			TspMultiStream();
			virtual ~TspMultiStream();

			int start(ThreadPool* thread_pool);

			void setRecvContextFactory(RecvContextFactory factory);

			int onRecvPacket(const std::string& remote_ip, int remote_port, std::unique_ptr<const char[]> packet_data, int packet_len);
			int processRecvPacket(const std::string& remote_ip, int remote_port, std::unique_ptr<const char[]>& packet_data, int packet_len);
		};

	} // namespace tsp

} // namespace grida

#endif // __GRIDA_TSP_TSP_MULTI_STREAM_HPP__
