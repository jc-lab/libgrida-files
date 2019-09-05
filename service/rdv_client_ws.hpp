/**
 * @file	rdv_client_ws.hpp
 * @author	Jichan (development@jc-lab.net / http://ablog.jc-lab.net/ )
 * @copyright Copyright (C) 2019 jichan.\n
 *            This software may be modified and distributed under the terms
 *            of the Apache License 2.0.  See the LICENSE file for details.
 */

#ifndef __GRIDA_SERVICE_RDV_CLIENT_WS_HPP__
#define __GRIDA_SERVICE_RDV_CLIENT_WS_HPP__

#include <uv.h>
#include <libwebsockets.h>

#include <memory>
#include <mutex>
#include <vector>

#include <libstomp-cpp/lws_client.hpp>

#ifdef _DEBUG
#include <assert.h>
#endif

namespace grida {
	namespace service {

		/**
		 * Websocket 클라이언트 구현
		 */
		class RdvClientWs
		{
		public:
			struct SendQueueData {
				virtual unsigned char* get_buffer() = 0;
				virtual size_t get_data_size() const = 0;
			};

			struct SendQueueDataVector {
				std::vector<unsigned char> buffer;
				size_t data_size;

				unsigned char* get_buffer() {
					if (buffer.size() < LWS_PRE)
						return NULL;
					return &buffer[LWS_PRE];
				}

				unsigned char *allocate(size_t size) {
					buffer.assign((size_t)(size + LWS_PRE + LWS_SEND_BUFFER_POST_PADDING), (const unsigned char&)0);
					return &buffer[LWS_PRE];
				}

				size_t get_buffer_size() const {
					return buffer.size() - LWS_PRE - LWS_SEND_BUFFER_POST_PADDING;
				}

				size_t get_data_size() const {
					return data_size;
				}

				void set_data_size(size_t size) {
					if (size > get_buffer_size())
						throw std::exception("size > get_buffer_size()");
					data_size = size;
				}
			};

		private:
			class MyClient;

			typedef struct ws_pss_wrapper {
				int inited;
				std::shared_ptr<MyClient>* obj;
			} ws_pss_wrapper_t;

			class MyClient : public stomp::LibwebsocketsClient
			{
			public:
				int onConnected(stomp::Frame* frame) override;
				int onMessage(stomp::Frame* frame) override;
				int onClosed() override;
			};

			void* foreign_loops[1];
			struct lws_context* ws_context;
			struct lws* ws_client_wsi;
			static const struct lws_protocols ws_protocols[1];
			static const struct lws_event_loop_ops ws_event_loop_ops;

			std::mutex session_lock_;
			std::shared_ptr<MyClient> stomp_client_;

			int connect_client(struct lws_context* context);
			static int callback_protocol(struct lws* wsi, enum lws_callback_reasons reason, void* user, void* in, size_t len);
			int protocol_handler(struct lws* wsi, enum lws_callback_reasons reason, void* user, void* in, size_t len);

#ifdef _DEBUG
			RdvClientWs(const RdvClientWs& o) { assert(false); }
#endif

		public:
			RdvClientWs();
			~RdvClientWs();

			int start(uv_loop_t *loop);
			int stop();
		};

	} // namespace service
} // namespace grida

#endif // __GRIDA_SERVICE_RDV_CLIENT_WS_HPP__
