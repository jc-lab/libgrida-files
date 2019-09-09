/**
 * @file	rdv_client_ws.cpp
 * @author	Jichan (development@jc-lab.net / http://ablog.jc-lab.net/ )
 * @copyright Copyright (C) 2019 jichan.\n
 *            This software may be modified and distributed under the terms
 *            of the Apache License 2.0.  See the LICENSE file for details.
 */

#include "rdv_client_ws.hpp"

#include <libstomp-cpp/command/subscribe.hpp>

namespace grida {
	namespace service {

		const struct lws_protocols RdvClientWs::ws_protocols[1] = {
			{
				"default",
				callback_protocol,
				sizeof(ws_pss_wrapper_t),
				10,
			}
		};

		RdvClientWs::RdvClientWs()
			: ws_context(NULL), ws_client_wsi(NULL)
		{
		}

		RdvClientWs::~RdvClientWs()
		{

		}

		int RdvClientWs::MyClient::onConnected(stomp::Frame* frame) {
			printf("onConnected\n");

			stomp::command::Subscribe subscribe(this);
			subscribe.destination("/topic/greetings");
			lwsl_user("subscribe /topic/greetings : %s\n", subscribe.id().c_str());
			this->sendCommand(&subscribe);

			return 0;
		}
		int RdvClientWs::MyClient::onMessage(stomp::Frame* frame) {
			printf("onMessage : %s\n", frame->command().c_str());
			return 0;
		}
		int RdvClientWs::MyClient::onClosed() {
			printf("onClosed\n");
			return 0;
		}

		int RdvClientWs::protocol_handler(struct lws* wsi, enum lws_callback_reasons reason, void* user, void* in, size_t len)
		{
			int rc;
			bool processed = false;

			ws_pss_wrapper_t* pss_wrapper = (ws_pss_wrapper_t*)user;
			struct lws_context* ctx = lws_get_context(wsi);

			switch (reason) {
			case LWS_CALLBACK_PROTOCOL_INIT:
				goto TRY_CONNECT;

			case LWS_CALLBACK_WSI_CREATE:
				if (pss_wrapper->obj)
				{
					break;
				}
				pss_wrapper->obj = new std::shared_ptr<MyClient>();
				pss_wrapper->obj->reset(new MyClient());
				{
					std::unique_lock<std::mutex> lock{ session_lock_ };
					stomp_client_ = (*pss_wrapper->obj);
				}
				pss_wrapper->inited = true;
				break;

			case LWS_CALLBACK_WSI_DESTROY:
				if (!pss_wrapper->obj) {
					break;
				}
				{
					std::unique_lock<std::mutex> lock{ session_lock_ };
					stomp_client_.reset();
				}
				delete pss_wrapper->obj;
				pss_wrapper->obj = NULL;
				pss_wrapper->inited = false;
				break;

			case LWS_CALLBACK_CLIENT_CONNECTION_ERROR:
				lwsl_err("CLIENT_CONNECTION_ERROR: %s\n",
					in ? (char*)in : "(null)");
				ws_client_wsi = NULL;
				lws_timed_callback_vh_protocol(lws_get_vhost(wsi),
					lws_get_protocol(wsi), LWS_CALLBACK_USER + 1, 1);
				break;

				/* --- client callbacks --- */

			case LWS_CALLBACK_CLIENT_APPEND_HANDSHAKE_HEADER:
				if (len >= 1024) {
					char** p = (char**)in;
					*p += snprintf(*p, 1024, "Authorization: TEST\n");
					processed = true;
				}
				else {
					return 1;
				}
				break;

			case LWS_CALLBACK_CLIENT_ESTABLISHED:
				lwsl_user("%s: established\n", __func__);
				break;

			case LWS_CALLBACK_WS_CLIENT_DROP_PROTOCOL:
				lwsl_err("LWS_CALLBACK_WS_CLIENT_DROP_PROTOCOL: %s\n",
					in ? (char*)in : "(null)");
				ws_client_wsi = NULL;
				lws_timed_callback_vh_protocol(lws_get_vhost(wsi),
					lws_get_protocol(wsi),
					LWS_CALLBACK_USER + 1, 1);
				break;

			case LWS_CALLBACK_CLIENT_RECEIVE_PONG:
				lwsl_user("LWS_CALLBACK_CLIENT_RECEIVE_PONG\n");
				lwsl_hexdump_notice(in, len);
				break;

			case LWS_CALLBACK_USER + 1:
				lwsl_notice("%s: LWS_CALLBACK_USER\n", __func__);
				goto TRY_CONNECT;
				break;

			default:
				break;
			}

			if (pss_wrapper) {
				if (pss_wrapper->obj) {
					rc = pss_wrapper->obj->get()->callbackProtocol(wsi, reason, user, in, len, &processed);
				}
			}
			if (processed)
				return rc;

			return lws_callback_http_dummy(wsi, reason, user, in, len);

		TRY_CONNECT:
			lwsl_user("TRY_CONNECT\n");
			if (connect_client(ctx))
				return lws_timed_callback_vh_protocol(lws_get_vhost(wsi),
					lws_get_protocol(wsi),
					LWS_CALLBACK_USER + 1, 1);
			else
				return 0;
		}

		int RdvClientWs::callback_protocol(struct lws* wsi, enum lws_callback_reasons reason,
			void* user, void* in, size_t len)
		{
			struct lws_context* ctx = lws_get_context(wsi);
			RdvClientWs* pthis = (RdvClientWs*)lws_context_user(ctx);
			return pthis->protocol_handler(wsi, reason, user, in, len);
		}

		int RdvClientWs::connect_client(struct lws_context* context)
		{
			struct lws_client_connect_info i;
			memset(&i, 0, sizeof(i));
			i.context = context;
			i.port = 8080;
			i.address = "127.0.0.1";
			i.path = "/grida-ws";
			i.host = i.address;
			i.origin = i.address;
			//i.ssl_connection = NULL;
			i.protocol = "default";
			i.local_protocol_name = "default";
			i.pwsi = &ws_client_wsi;

			return !lws_client_connect_via_info(&i);
		}

		int RdvClientWs::start(uv_loop_t* loop)
		{
			lws_context_creation_info info = {0};
			info.options = LWS_SERVER_OPTION_DO_SSL_GLOBAL_INIT | LWS_SERVER_OPTION_LIBUV;
			info.port = CONTEXT_PORT_NO_LISTEN;
			info.protocols = ws_protocols;
			//info.client_ssl_ca_filepath = 0;
			info.user = this;
			foreign_loops[0] = loop;
			info.foreign_loops = foreign_loops;
			info.count_threads = 1;
			ws_context = lws_create_context(&info);
			return 0;
		}

		int RdvClientWs::stop()
		{
			return 0;
		}

	} // namespace service
} // namespace grida

