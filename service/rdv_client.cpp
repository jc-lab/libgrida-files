/**
 * @file	rdv_client.cpp
 * @author	Jichan (development@jc-lab.net / http://ablog.jc-lab.net/ )
 * @copyright Copyright (C) 2019 jichan.\n
 *            This software may be modified and distributed under the terms
 *            of the Apache License 2.0.  See the LICENSE file for details.
 */

#include "rdv_client.hpp"

#include "rdv_client_ws.hpp"

#include "../peer_context.hpp"

#include <uvw/timer.hpp>

#include "route_tracer.hpp"

#include <thread>
#include <unordered_set>

namespace grida {

	namespace service {

		class RdvClient::Impl {
		public:
			struct ObjectWrapper {
				virtual ~ObjectWrapper() {}
			};
			template<class T>
			struct NormalObjectWrapper : public ObjectWrapper {
				T obj;
				template<typename ...Args>
				NormalObjectWrapper(Args ... args)
					: obj(args)
				{ }
			};
			template<>
			struct NormalObjectWrapper<std::thread> : public ObjectWrapper {
				std::thread obj;
				template<typename ...Args>
				NormalObjectWrapper(Args ... args)
					: obj(args...)
				{ }

				virtual ~NormalObjectWrapper() {
					obj.join();
				}
			};

			std::shared_ptr<::uvw::Loop> loop_;
			std::shared_ptr<NativeLoop> native_loop_;
			PeerContext* peer_context_;

			std::shared_ptr<RdvClientWs> ws_;
			RouteTracer* route_tracer_;

			std::shared_ptr<::uvw::TimerHandle> route_trace_timer_;

			std::unordered_set< std::unique_ptr<ObjectWrapper> > managed_objects_;

			Impl(RdvClient *parent, std::shared_ptr<NativeLoop> native_loop, PeerContext *peer_context)
				: route_tracer_(NULL), loop_(parent->loop_), native_loop_(native_loop), peer_context_(peer_context)
			{
			}

			~Impl()
			{
			}

			int start(RouteTracer* route_tracer)
			{
				int rc;

				if (ws_.get()) {
					ws_->stop();
				}
				ws_ = RdvClientWs::create();

				route_tracer_ = route_tracer;

				rc = ws_->start(native_loop_);
				if (rc)
					return rc;

				//TODO: Use async
				/*
				route_trace_timer_ = loop_->resource<::uvw::TimerHandle>();
				route_trace_timer_->on<::uvw::TimerEvent>([this](::uvw::TimerEvent& e, ::uvw::TimerHandle& handle) {
					std::unique_ptr<NormalObjectWrapper<std::thread> > obj(new NormalObjectWrapper<std::thread>(tracertThreadProc, this));
					managed_objects_.insert(std::move(obj));
					});
				route_trace_timer_->start(std::chrono::milliseconds::duration(1000), std::chrono::milliseconds::duration(300000));
                */
				return rc;
			}

			int stop()
			{
				if (ws_.get()) {
					return ws_->stop();
				}
				ws_.reset();
				return 0;
			}

		private:
			static int tracertThreadProc(Impl* pthis) {
				std::unique_ptr< std::list<RouteTracer::TestResultItem> > result_list;
				pthis->route_tracer_->tracert(result_list, "8.8.8.8");

				return 0;
			}
		};

		RdvClient::RdvClient(const internal::LoopProvider* provider, std::shared_ptr<NativeLoop> native_loop, PeerContext* peer_context)
			: LoopUse(provider), impl_(new Impl(this, native_loop, peer_context))
		{
		}

		RdvClient::~RdvClient()
		{

		}

		int RdvClient::start(RouteTracer *route_tracer)
		{
			if (checkUseableWebsockets()) {
				return impl_->start(route_tracer);
			}
			return 0;
		}

		int RdvClient::stop()
		{
			return impl_->stop();
		}


	} // namespace service
} // namespace grida

