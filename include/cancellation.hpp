#pragma once
#ifndef MYLIB_COROUTINE_CANCELLATION_H
#define MYLIB_COROUTINE_CANCELLATION_H 1

#include <concepts>
#include <coroutine>
#include <exception>
#include <utility>

namespace mylib {

    using stopped_handler_type = std::coroutine_handle<>(*)(void*) noexcept;

    [[noreturn]]
    inline std::coroutine_handle<> default_stopped_handler(void*) noexcept {
        std::terminate();
    }

    template<typename PromiseType>
    concept has_unhandled_stopped = requires(PromiseType& p) {
        { p.unhandled_stopped() } noexcept -> std::convertible_to<std::coroutine_handle<>>;
    };

    template<has_unhandled_stopped PromiseType>
    inline std::coroutine_handle<> forward_stopped_handler(void* handle_ptr) noexcept {
        return std::coroutine_handle<PromiseType>::from_address(handle_ptr)
            .promise().unhandled_stopped();
    }

    class cancellation_base
    {
    public:
        template<typename OtherPromise>
        std::coroutine_handle<> set_continuation(std::coroutine_handle<OtherPromise> c) noexcept {
            if constexpr (mylib::has_unhandled_stopped<OtherPromise>) {
                this->stopped_handler = &mylib::forward_stopped_handler<OtherPromise>;
            } else {
                this->stopped_handler = &mylib::default_stopped_handler;
            }
            return std::exchange(this->continuation, c);
        }

        std::coroutine_handle<> get_continuation() const noexcept {
            return this->continuation;
        }

        std::coroutine_handle<> unhandled_stopped() noexcept {
            return this->stopped_handler(continuation.address());
        }

    private:
        std::coroutine_handle<> continuation = std::noop_coroutine();
        stopped_handler_type stopped_handler = &mylib::default_stopped_handler;
    };

} // namespace mylib

#endif
