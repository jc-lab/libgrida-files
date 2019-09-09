/**
 * @file	mcd_service_impl.hpp
 * @author	Jichan (development@jc-lab.net / http://ablog.jc-lab.net/ )
 * @copyright Copyright (C) 2019 jichan.\n
 *            This software may be modified and distributed under the terms
 *            of the Apache License 2.0.  See the LICENSE file for details.
 */

#ifndef __GRIDA_SERVICE_MCD_SERVICE_IMPL_HPP__
#define __GRIDA_SERVICE_MCD_SERVICE_IMPL_HPP__

#include "mcd_service.hpp"

#include "../tsp/tsp_multi_stream.hpp"
#include "../tsp/pske_protocol.hpp"

#include "mcd/mcd_protocol.hpp"

#include <uvw/udp.hpp>
#include <uvw/timer.hpp>

#include <mutex>

namespace grida {
    namespace service {

		class McdService::TspLayer : public tsp::TspMultiStream
		{
		private:
			Impl* impl_;

		public:
			std::shared_ptr<uvw::UDPHandle> send_socket_;
			std::shared_ptr<uvw::UDPHandle> recv_socket_;
			std::string multicast_addr_;

			TspLayer(Impl* impl);
			void openSender(::uvw::Loop* loop, const std::string& multicast_addr, const std::string& interface_addr);
			void openReceiver(::uvw::Loop* loop, const std::string& multicast_addr, const std::string& interface_addr, int local_port);
			int open(::uvw::Loop* loop, const std::string& multicast_addr, const std::string& interface_addr = "");
			int sendPacket(std::unique_ptr<char[]> packet_data, int packet_len);
			int local_port() const;
			int onRecvEndPayload(const std::vector<std::unique_ptr<tsp::Payload>>& ancestors, std::unique_ptr<tsp::Payload>& payload) override;
		};

        class McdService::Impl {
        private:
            PeerContext* peer_context_;
            const internal::LoopProvider* loop_provider_;

            std::unique_ptr<tsp::PskeProtocol> protocol_pske_;

			struct DiscoveryContextItem {
				std::mutex mutex;
				std::chrono::time_point<std::chrono::steady_clock> created_at;
				std::map<ObjectDiscoveryContext*, std::shared_ptr<ObjectDiscoveryContext>> map;

				DiscoveryContextItem() {
					created_at = std::chrono::steady_clock::now();
				}
			};

			struct {
				std::mutex mutex;
				std::map<std::string, DiscoveryContextItem> map;

				DiscoveryContextItem& get(const std::string& object_id) {
					std::unique_lock<std::mutex> lock(mutex);
					return map[object_id];
				}

				DiscoveryContextItem* find(const std::string& object_id) {
					std::unique_lock<std::mutex> lock(mutex);
					auto iter = map.find(object_id);
					if (iter == map.end())
						return NULL;
					return &(iter->second);
				}

				void remove(ObjectDiscoveryContext* discovery_context) {
					std::unique_lock<std::mutex> lock(mutex);
					auto iter_obj = map.find(discovery_context->object_id());
					if (iter_obj != map.end())
					{
						DiscoveryContextItem& item = iter_obj->second;
						auto iter_ctx = item.map.find(discovery_context);
						if (iter_ctx != item.map.end()) {
							item.map.erase(iter_ctx);
						}
						if (item.map.empty()) {
							map.erase(iter_obj);
						}
					}
				}
			} discovery_contexts_;

        public:
            TspLayer transport_;
            mcd::McdProtocol protocol_mcd_;
			std::shared_ptr<uvw::TimerHandle> discovery_timer_;

        public:
            Impl(PeerContext *peer_context, const internal::LoopProvider* loop_provider);
            ~Impl();
            int start(ThreadPool* thread_pool, const std::string& multicast_ip, const std::string& interface_ip);
            int stop();
            int onRecvEndPayload(const std::vector<std::unique_ptr<tsp::Payload>>& ancestors, std::unique_ptr<tsp::Payload>& payload);
			int sendMcdPayload(mcd::McdPayload* payload, tsp::PskeFlags pske_flags, const std::vector<std::unique_ptr<grida::tsp::Payload>>* ancestors = NULL);
			int sendMcdPayload(mcd::McdPayload* payload, int pske_flags, const std::vector<std::unique_ptr<grida::tsp::Payload>>* ancestors = NULL) {
				return sendMcdPayload(payload, (tsp::PskeFlags)pske_flags, ancestors);
			}

			int startDiscoveryObject(std::shared_ptr<ObjectDiscoveryContext> discovery_context);
			int stopDiscoveryObject(ObjectDiscoveryContext *discovery_context);
        };

    } // namespace service
} // namespace grida

#endif // __GRIDA_SERVICE_MCD_SERVICE_IMPL_HPP__
