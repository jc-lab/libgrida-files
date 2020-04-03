/**
 * @file	native_loop.hpp
 * @author	Jichan (development@jc-lab.net / http://ablog.jc-lab.net/ )
 * @date	2020/04/02
 * @copyright Copyright (C) 2019 jichan.\n
 *            This software may be modified and distributed under the terms
 *            of the Apache License 2.0.  See the LICENSE file for details.
 */

#ifndef __GRIDA_NATIVE_LOOP_HPP__
#define __GRIDA_NATIVE_LOOP_HPP__

#include <memory>
#include <thread>
#include <future>
#include <functional>

#include <uv.h>

namespace grida {

	class NativeLoop {
	private:
		std::weak_ptr<NativeLoop> self_;
		NativeLoop();

		std::thread th_;
		std::shared_future<void> exit_future_;
		uv_loop_t loop_;

		static void handle_walk(uv_handle_t* handle, void* arg);

	public:
		~NativeLoop();

		static std::shared_ptr<NativeLoop> create();

		void start();
		std::future_status stop(int timeout_ms);

		uv_loop_t* getLoop() {
			return &loop_;
		}

		template<class THandle>
		std::shared_ptr<THandle> resource() {
			std::shared_ptr<NativeLoop> self(self_.lock());
			std::shared_ptr<THandle> handle(new THandle(self));
			handle->shared_self_ = handle;
			return handle;
		}

		// Managed Handle
		class BaseHandle {
		private:
			bool closing_;
			std::shared_ptr<BaseHandle> shared_self_;
			std::shared_ptr<NativeLoop> loop_;
			static void uvCloseCallback(uv_handle_t* handle);

		protected:
			friend class NativeLoop;
			virtual uv_handle_t* getUvHandle() = 0;

			std::shared_ptr<BaseHandle> getSelfBase() {
				return shared_self_;
			}

		public:
			BaseHandle(std::shared_ptr<NativeLoop> loop) : loop_(loop), closing_(false) {}
			virtual ~BaseHandle() {}
			
			void close();
			bool isClosing() const {
				return closing_;
			}

			std::shared_ptr<NativeLoop> loop() {
				return loop_;
			}
		};

		class BaseEvent {
		public:
			virtual ~BaseEvent() {}
		};

		template <class TEvent, class THandle>
		using BaseHandlerType = std::function<void(TEvent& evt, THandle& handle)>;

		class AsyncEvent : public BaseEvent { };
		class AsyncHandle : public BaseHandle {
		public:
			typedef BaseHandlerType<AsyncEvent, AsyncHandle> HandlerType;

		private:
			uv_async_t uv_handle_;
			HandlerType handler_;

			uv_handle_t* getUvHandle() override {
				return (uv_handle_t *)&uv_handle_;
			}

			static void uvCallbackHandler(uv_async_t* handle);

		public:
			AsyncHandle(std::shared_ptr<NativeLoop> loop);
			virtual ~AsyncHandle() {}
			void on(HandlerType&& handler);
			int send();
		};
	};

} // namespace grida

#endif // __GRIDA_NATIVE_LOOP_HPP__
