/**
 * @file	mcd_service_impl.cpp
 * @author	Jichan (development@jc-lab.net / http://ablog.jc-lab.net/ )
 * @copyright Copyright (C) 2019 jichan.\n
 *            This software may be modified and distributed under the terms
 *            of the Apache License 2.0.  See the LICENSE file for details.
 */

#include "mcd_service_impl.hpp"

#include <uvw/timer.hpp>
#include <uvw/async.hpp>

#include <thread>

#include "../data/bitstream.hpp"
#include "../internal/hex_util.hpp"

#include <set>

namespace grida {

    namespace service {

		McdService::TspLayer::TspLayer(Impl* impl)
			: impl_(impl)
		{
		}

        McdService::Impl::Impl(PeerContext *peer_context, const internal::LoopProvider* loop_provider) :
        peer_context_(peer_context), loop_provider_(loop_provider), transport_(this)
        {
            transport_.setEndpayloadType(protocol_mcd_.get_sp_type());
            transport_.addProtocol(&protocol_mcd_);
        }

        McdService::Impl::~Impl()
        {

        }

        int McdService::Impl::start(ThreadPool *thread_pool, const std::string& multicast_ip, const std::string& interface_ip) {
			std::shared_ptr<uvw::Loop> loop(loop_provider_->get_loop());

            protocol_pske_ = peer_context_->createMcdPskeProtocol();
            transport_.addProtocol(protocol_pske_.get());
            transport_.setRecvContextFactory([this](const std::string& remote_ip, int remote_port) {
				return peer_context_->createMcdTspRecvContext(remote_ip, remote_port);
            });

            transport_.open(loop.get(), multicast_ip, interface_ip);
            transport_.start(thread_pool);

			discovery_timer_ = loop->resource<uvw::TimerHandle>();
			discovery_timer_->on<uvw::TimerEvent>([this, thread_pool](const uvw::TimerEvent& evt, uvw::TimerHandle& handle) {
				thread_pool->send([this]() {
					std::unique_lock<std::mutex> lock(discovery_contexts_.mutex);
					std::chrono::time_point<std::chrono::steady_clock> now = std::chrono::steady_clock::now();
					for (auto iter = discovery_contexts_.map.begin(); iter != discovery_contexts_.map.end(); iter++) {
						auto diff = std::chrono::duration_cast<std::chrono::milliseconds>(now - iter->second.created_at);
						if (diff.count() >= 1000) {
							mcd::McdObjectDiscoveryRequestPayload payload;
							payload.object_id.set(iter->first);
							sendMcdPayload(&payload, (tsp::PskeFlags)(grida::tsp::PSKE_FLAG_BROADCAST | grida::tsp::PSKE_FLAG_IPE));
						}
					}
				});
			});
			discovery_timer_->start(uvw::TimerHandle::Time{ 10000 }, uvw::TimerHandle::Time{ 10000 });

            return 0;
        }

        int McdService::Impl::stop() {
            return 0;
        }

        int McdService::Impl::onRecvEndPayload(const std::vector<std::unique_ptr<tsp::Payload>>& ancestors, std::unique_ptr<tsp::Payload>& payload) {
            mcd::McdPayload* base_payload = dynamic_cast<mcd::McdPayload*>(payload.get());
            uint16_t payload_type = base_payload->payload_type();

			tsp::SocketPayload* socket_payload = NULL;
            tsp::PskePayload* pske_payload = NULL;
            for (auto iter = ancestors.cbegin(); iter != ancestors.cend(); iter++) {
				if (!pske_payload) {
					pske_payload = dynamic_cast<tsp::PskePayload*>(iter->get());
				}
				if (!socket_payload) {
					socket_payload = dynamic_cast<tsp::SocketPayload*>(iter->get());
				}
                if (pske_payload && socket_payload)
                    break;
            }

            if (payload_type == mcd::McdObjectDiscoveryRequestPayload::PAYLOAD_TYPE) {
                mcd::McdObjectDiscoveryRequestPayload* real_payload = dynamic_cast<mcd::McdObjectDiscoveryRequestPayload*>(payload.get());
				std::unique_ptr<DBObjectInformation> object_row;
				std::list<std::unique_ptr<DBPieceInformation>> piece_rows;

				peer_context_->dbGetObjectInformation(object_row, real_payload->object_id.get(), NULL);
				if (object_row) {
					SeedFile seed_file_vo;
					std::set<std::string> local_pieces_set;
					mcd::McdObjectDiscoveryResponsePayload response_payload;
					data::BitStream pieces_map;

					peer_context_->dbGetAllPieceInformation(piece_rows, real_payload->object_id.get(), NULL);

					for (auto row_iter = piece_rows.cbegin(); row_iter != piece_rows.cend(); row_iter++) {
						grida::DBPieceInformation *row = row_iter->get();
						local_pieces_set.insert(row->piece_id);
					}

					response_payload.object_id.set(real_payload->object_id.get());

					try {
						auto& pieces_list = seed_file_vo.pieces.get();

						seed_file_vo.deserialize(object_row->raw_seed_file, 0);
						pieces_map.init(seed_file_vo.pieces.get().size());

						for (auto piece_iter = pieces_list.cbegin(); piece_iter != pieces_list.cend(); piece_iter++)
						{
							std::string piece_id = ::grida::internal::HexUtil::bytesToHexString(piece_iter->data(), piece_iter->size());
							auto found_piece = local_pieces_set.find(piece_id);
							if (found_piece != local_pieces_set.end())
							{
								pieces_map.push(true);
							} else {
								pieces_map.push(false);
							}
						}

						response_payload.pieces_bitmap.set(pieces_map.ref_buffer());

						this->sendMcdPayload(&response_payload, tsp::PSKE_FLAG_IPE_RESPONSE | tsp::PSKE_FLAG_IPE | tsp::PSKE_FLAG_ENCRYPTION, &ancestors);
					} catch (JsBsonRPC::Serializable::ParseException& e) {

					}
				}
            }
            else if (payload_type == mcd::McdObjectDiscoveryResponsePayload::PAYLOAD_TYPE) {
				mcd::McdObjectDiscoveryResponsePayload* real_payload = dynamic_cast<mcd::McdObjectDiscoveryResponsePayload*>(payload.get());
				DiscoveryContextItem * pdci = discovery_contexts_.find(real_payload->object_id.get());
				if (pdci)
				{
					std::unique_lock<std::mutex> lock(pdci->mutex);
					for (auto iter = pdci->map.begin(); iter != pdci->map.end(); iter++)
					{
						iter->second->onObjectDiscovered(socket_payload, real_payload);
					}
				}
            }
            else {

            }

            return 1;
        }

		int McdService::Impl::sendMcdPayload(mcd::McdPayload* payload, tsp::PskeFlags pske_flags, const std::vector<std::unique_ptr<grida::tsp::Payload>>* ancestors)
		{
			std::unique_ptr<char[]> out_packet;
			std::unique_ptr<tsp::Payload> out_payload;
			int rc;

			rc = protocol_mcd_.makePacket(out_packet, payload, NULL);
			if (rc <= 0)
				return rc;

			if (ancestors && (pske_flags & tsp::PSKE_FLAG_IPE)) {
				pske_flags = (tsp::PskeFlags)(pske_flags | tsp::PSKE_FLAG_IPE_RESPONSE);
			}

			rc = protocol_pske_->wrap(out_payload, out_packet.get(), rc, pske_flags, ancestors);
			if (rc <= 0)
				return rc;

			rc = protocol_pske_->makePacket(out_packet, out_payload.get(), NULL);
			if (rc <= 0)
				return rc;

			rc = transport_.sendPacket(std::move(out_packet), rc);

			return rc;
		}

		int McdService::Impl::startDiscoveryObject(std::shared_ptr<ObjectDiscoveryContext> discovery_context)
		{
			int rc;
			mcd::McdObjectDiscoveryRequestPayload payload;

			{
				DiscoveryContextItem& dci = discovery_contexts_.get(discovery_context->object_id());
				std::unique_lock<std::mutex> lock(dci.mutex);
				dci.map[discovery_context.get()] = discovery_context;
			}

			payload.object_id.set(discovery_context->object_id());

			sendMcdPayload(&payload, (tsp::PskeFlags)(grida::tsp::PSKE_FLAG_BROADCAST | grida::tsp::PSKE_FLAG_IPE));

			return 0;
		}

		int McdService::Impl::stopDiscoveryObject(ObjectDiscoveryContext* discovery_context)
		{
			discovery_contexts_.remove(discovery_context);
			return 0;
		}

        void McdService::TspLayer::openSender(::uvw::Loop* loop, const std::string& multicast_addr, const std::string& interface_addr) {
            bool uv_res;
            send_socket_ = loop->resource<uvw::UDPHandle>();
            uv_res = send_socket_->multicastMembership(multicast_addr, interface_addr, uvw::UDPHandle::Membership::JOIN_GROUP);
        }

        void McdService::TspLayer::openReceiver(::uvw::Loop* loop, const std::string& multicast_addr, const std::string& interface_addr, int local_port) {
            bool uv_res;
            recv_socket_ = loop->resource<uvw::UDPHandle>();
			recv_socket_->on<uvw::ErrorEvent>([](const uvw::ErrorEvent& evt, auto& handle) {
				printf("recv_socket_ error : %d %s\n", evt.code(), evt.name());
			});
            recv_socket_->on<uvw::UDPDataEvent>([this, local_port](uvw::UDPDataEvent& event, uvw::UDPHandle& handle) {
                if (event.partial) {
                    printf("ERROR: partial packet\n");
                }

                printf("EVENT RECV %s:%d:%d\n", event.sender.ip.c_str(), event.sender.port, local_port);
                onRecvPacket(event.sender.ip, event.sender.port, std::move(event.data), event.length);
            });

			recv_socket_->bind("0.0.0.0", 19800, uvw::UDPHandle::Bind::REUSEADDR);
			uv_res = recv_socket_->multicastMembership(multicast_addr, interface_addr, uvw::UDPHandle::Membership::JOIN_GROUP);
			uv_res = recv_socket_->multicastLoop(true);
            recv_socket_->recv();
        }

        int McdService::TspLayer::open(::uvw::Loop* loop, const std::string& multicast_addr, const std::string& interface_addr) {
            multicast_addr_ = multicast_addr;
            openSender(loop, multicast_addr, interface_addr);
            openReceiver(loop, multicast_addr, interface_addr, send_socket_->sock().port);
            return 1;
        }

        int McdService::TspLayer::local_port() const {
            return send_socket_->sock().port;
        }

		int McdService::TspLayer::sendPacket(std::unique_ptr<char[]> packet_data, int packet_len) {
			std::shared_ptr<uvw::AsyncHandle> task = send_socket_->loop().resource<uvw::AsyncHandle>();

			auto runnable = ThreadPool::taskSharedBind([this](std::unique_ptr<char[]> packet_data, int packet_len) {
				send_socket_->send(multicast_addr_, 19800, std::move(packet_data), packet_len);
			}, std::move(packet_data), packet_len);

			task->on<uvw::AsyncEvent>([runnable](const uvw::AsyncEvent& evt, uvw::AsyncHandle& handle) {
				runnable->run();
				handle.close();
			});
			task->send();
			
			return packet_len;
		}

        int McdService::TspLayer::onRecvEndPayload(const std::vector<std::unique_ptr<tsp::Payload>>& ancestors, std::unique_ptr<tsp::Payload>& payload) {
            return impl_->onRecvEndPayload(ancestors, payload);
        }

    } // namespace service
} // namespace grida

