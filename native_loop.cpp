/**
 * @file	native_loop.cpp
 * @author	Jichan (development@jc-lab.net / http://ablog.jc-lab.net/ )
 * @date	2020/04/02
 * @copyright Copyright (C) 2019 jichan.\n
 *            This software may be modified and distributed under the terms
 *            of the Apache License 2.0.  See the LICENSE file for details.
 */

#include "native_loop.hpp"

#include <vector>

namespace grida {

	NativeLoop::NativeLoop() {
		uv_loop_init(&loop_);
	}

	NativeLoop::~NativeLoop() {
		if (th_.joinable()) {
			th_.detach();
		}
	}

	std::shared_ptr<NativeLoop> NativeLoop::create() {
		std::shared_ptr<NativeLoop> instance(new NativeLoop());
		instance->self_ = instance;
		return instance;
	}

	void NativeLoop::start() {
		std::shared_ptr<NativeLoop> self(self_.lock());
		std::promise<void> promise;
		exit_future_ = promise.get_future().share();
		th_ = std::thread([self, promise = std::move(promise)]() mutable -> void {
			uv_run(&(self->loop_), UV_RUN_DEFAULT);
			uv_loop_close(&self->loop_);
			promise.set_value();
		});
	}

	void NativeLoop::handle_walk(uv_handle_t* handle, void* arg) {
		fprintf(stderr, "HAS OPENED HANDLE: %p\n", handle);
		uv_close(handle, [](uv_handle_t * handle) -> void {
			fprintf(stderr, "HAS OPENED HANDLE: %p: CLOSED\n", handle);
		});
	}

	std::future_status NativeLoop::stop(int timeout_ms) {
		if (th_.joinable()) {
			if (timeout_ms >= 0) {
				std::future_status status = exit_future_.wait_for(std::chrono::milliseconds{ timeout_ms });
				if (status != std::future_status::ready) {
					uv_walk(&loop_, handle_walk, this);
				}
				return status;
			}
			th_.join();
		}
		return std::future_status::ready;
	}

	void NativeLoop::BaseHandle::uvCloseCallback(uv_handle_t* handle) {
		std::shared_ptr<BaseHandle> self((static_cast<BaseHandle*>(handle->data))->shared_self_);
		self->shared_self_.reset();
	}

	void NativeLoop::BaseHandle::close() {
		if (!closing_) {
			closing_ = true;
			uv_close(getUvHandle(), uvCloseCallback);
		}
	}

	NativeLoop::AsyncHandle::AsyncHandle(std::shared_ptr<NativeLoop> loop)
		: BaseHandle(loop)
	{
		uv_async_init(loop->getLoop(), &uv_handle_, uvCallbackHandler);
		uv_handle_.data = this;
	}

	void NativeLoop::AsyncHandle::on(HandlerType&& handler) {
		handler_ = std::move(handler);
	}

	int NativeLoop::AsyncHandle::send() {
		return uv_async_send(&uv_handle_);
	}

	void NativeLoop::AsyncHandle::uvCallbackHandler(uv_async_t* handle) {
		std::shared_ptr<AsyncHandle> self(std::static_pointer_cast<AsyncHandle>(((AsyncHandle*)handle->data)->shared_self_));
		if (self->handler_) {
			AsyncEvent evt;
			self->handler_(evt, *self);
		}
		self->close();
	}

} // namespace grida
