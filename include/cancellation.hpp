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
    inline std::coroutine_handle<> null_stopped_handler(void*) noexcept {
        std::terminate();
    }

    template<typename PromiseType>
    concept has_unhandled_stopped = requires(PromiseType& p) {
        { p.unhandled_stopped() } noexcept -> std::convertible_to<std::coroutine_handle<>>;
    };

    // The default stopped handler which is set up during set continuation.
    // Forward the cancellation handle from caller of this coroutine.
    // Would not apply another layer of stopped handling.
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
                this->stopped_handler = &mylib::null_stopped_handler;
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
        stopped_handler_type stopped_handler = &mylib::null_stopped_handler;
    };

    class cancellation_task
    {
    public:
        struct promise_type
        {
            cancellation_task get_return_object() noexcept {
                return cancellation_task(std::coroutine_handle<promise_type>::from_promise(*this));
            }

            std::suspend_always initial_suspend() noexcept { return {}; }

            struct final_awaiter {
                constexpr bool await_ready() const noexcept { return false; }
                
                std::coroutine_handle<> await_suspend(std::coroutine_handle<promise_type> h) noexcept {
                    std::coroutine_handle<> c = h.promise().continuation;
                    h.destroy();
                    return c;
                }

                constexpr void await_resume() noexcept { std::unreachable(); }
            };

            final_awaiter final_suspend() noexcept { return {}; }

            void return_void() noexcept {}
            void unhandled_exception() noexcept { std::terminate(); }

            void set_continuation(std::coroutine_handle<> c) noexcept {
                continuation = c;
            }

            std::coroutine_handle<> continuation = std::noop_coroutine();
        };

        cancellation_task() = delete;
        cancellation_task(const cancellation_task&) = delete;
        cancellation_task& operator=(const cancellation_task&) = delete;

        void swap(cancellation_task& other) noexcept {
            std::swap(handle, other.handle);
        }

        cancellation_task(cancellation_task&& other) noexcept
            : handle(std::exchange(other.handle, {}))
        {}

        cancellation_task& operator=(cancellation_task&& other) noexcept {
            auto(std::move(other)).swap(*this);
            return *this;
        }

        std::coroutine_handle<> chain_cancel(std::coroutine_handle<> caller) noexcept {
            handle.promise().set_continuation(caller);
            return std::exchange(handle, nullptr);
        }

        ~cancellation_task() noexcept {
            if (handle) {
                handle.destroy();
            }
        }

    private:
        friend promise_type;
        explicit cancellation_task(std::coroutine_handle<promise_type> handle) noexcept
            : handle(handle)
        {}

        std::coroutine_handle<promise_type> handle = nullptr;
    };

} // namespace mylib

#endif
