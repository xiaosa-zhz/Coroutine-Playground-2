#ifndef MYLIB_CALLCC_H
#define MYLIB_CALLCC_H 1

#include <concepts>
#include <coroutine>
#include <utility>
#include <memory>

#include "task.hpp"

namespace mylib {

    // Forward declaration of mylib::callcc_task
    template<typename ReturnType>
    class callcc_task;

    // Forward declaration of mylib::cc
    template<typename ReturnType>
    class cc;

    // Forward declaration of mylib::cc_awaiter
    template<typename ReturnType>
    class [[nodiscard]] cc_awaiter;

    namespace details {

        // Forward declaration of mylib::details::cc_calling_base
        template<typename ReturnType>
        class cc_calling_base;

        struct get_cc_tag {};

        template<typename CallccTaskType>
        class callcc_promise : public mylib::details::task_promise<CallccTaskType>
        {
        public:
            using callcc_task_type = CallccTaskType;
            using handle_type = std::coroutine_handle<callcc_promise>;
            using return_type = typename callcc_task_type::return_type;
            using cc_type = cc<return_type>;
            
            callcc_task_type get_return_object() { return callcc_task_type(handle_type::from_promise(*this)); }

            // Forward other awaiters transparently
            template<typename T>
            T&& await_transform(T&& t) const noexcept { return std::forward<T>(t); }

            struct [[nodiscard]] get_cc_awaiter
            {
                bool await_ready() const noexcept { return true; }
                void await_suspend(std::coroutine_handle<>) const noexcept { std::unreachable(); }

                cc_type await_resume() const noexcept { return this->current_continuation; }

            private:
                friend callcc_promise;
                explicit get_cc_awaiter(const cc_type& c) noexcept : current_continuation(c) {}

                cc_type current_continuation;
            };

            get_cc_awaiter await_transform(get_cc_tag) noexcept {
                return get_cc_awaiter{ cc_type(handle_type::from_promise(*this)) };
            }

        private:
            friend cc_awaiter<return_type>;
            std::coroutine_handle<> get_continuation() const noexcept {
                return this->continuation;
            }
        };

        template<typename ReturnType>
        using promise_from_return_type = callcc_promise<callcc_task<ReturnType>>;

        template<typename ReturnType>
        using handle_from_return_type = std::coroutine_handle<promise_from_return_type<ReturnType>>;

    } // namespace mylib::details

    template<typename ReturnType>
    class [[nodiscard]] cc_awaiter
    {
    public:
        using return_type = ReturnType;
        using handle_type = details::handle_from_return_type<return_type>;

        bool await_ready() const noexcept { return false; }

        template<typename PromiseType>
        std::coroutine_handle<> await_suspend(std::coroutine_handle<PromiseType> current) noexcept {
            // resume to the caller of callcc_task
            return this->handle.promise().get_continuation();
        }

        void await_resume() const noexcept { std::unreachable(); }

    private:
        friend cc<return_type>;
        friend details::cc_calling_base<return_type>;
        explicit cc_awaiter(handle_type h) noexcept : handle(h) {}

        handle_type handle;
    };

    namespace details {

        template<typename ReturnType>
        class cc_calling_base
        {
        public:
            using return_type = ReturnType;
            using awaiter = cc_awaiter<return_type>;
        private:
            constexpr static bool return_reference = std::is_reference_v<return_type>;
        public:
            awaiter operator() (this auto&& self, return_type rt) noexcept requires (return_reference) {
                self.handle.promise().return_value(rt);
                return awaiter{ self.handle };
            }

            template<typename U = return_type>
                requires (not return_reference) && std::convertible_to<U, return_type> && std::constructible_from<return_type, U>
            awaiter operator() (this auto&& self, U&& rt) noexcept(std::is_nothrow_constructible_v<return_type, U>) {
                self.handle.promise().return_value(std::forward<U>(rt));
                return awaiter{ self.handle };
            }
        };

        template<typename Void>
            requires (std::is_void_v<Void>)
        class cc_calling_base<Void>
        {
        public:
            using return_type = void;
            using awaiter = cc_awaiter<void>;

            awaiter operator() (this auto&& self) noexcept {
                return awaiter{ self.handle };
            }
        };

    } // namespace mylib::details

    consteval details::get_cc_tag get_cc() noexcept { return {}; }

    template<typename ReturnType>
    class cc : public details::cc_calling_base<ReturnType>
    {
    public:
        using return_type = ReturnType;
        using handle_type = details::handle_from_return_type<return_type>;
        using promise_type = details::promise_from_return_type<return_type>;
        using exception_type = typename promise_type::exception_type;
    private:
        using base = details::cc_calling_base<return_type>;
        friend base;
    public:
        using awaiter = typename base::awaiter;

        cc() = delete;
        cc(const cc&) = default;
        cc& operator=(const cc&) = default;

        awaiter call_with_exception(exception_type ex) noexcept {
            this->handle.promise().unhandled_exception(std::move(ex));
            return awaiter{ this->handle };
        }

    private:
        friend promise_type;
        explicit cc(handle_type h) noexcept : handle(h) {}

        handle_type handle;
    };

    template<typename ReturnType>
    class [[nodiscard]] callcc_task
    {
    public:
        using return_type = ReturnType;
        using promise_type = details::callcc_promise<callcc_task>;
        using handle_type = typename promise_type::handle_type;
        using callcc_task_awaiter = mylib::details::task_awaiter<callcc_task>;

        callcc_task_awaiter operator co_await() && noexcept {
            return callcc_task_awaiter(std::exchange(this->coroutine, nullptr));
        }

        callcc_task(const callcc_task&) = delete;
        callcc_task& operator=(const callcc_task&) = delete;

        callcc_task(callcc_task&& other) noexcept
            : coroutine(std::exchange(other.coroutine, nullptr))
        {}

        callcc_task& operator=(callcc_task&& other) noexcept {
            callcc_task().swap(other);
            return *this;
        }

        void swap(callcc_task& other) noexcept {
            if (this == std::addressof(other)) return;
            std::ranges::swap(this->coroutine, other.coroutine);
        }

        ~callcc_task() { if (this->coroutine) { this->coroutine.destroy(); } }

    private:
        friend promise_type;
        callcc_task() = default;
        explicit callcc_task(handle_type handle) noexcept : coroutine(handle) {}

        handle_type coroutine = nullptr;
    };

} // namespace mylib

#endif // MYLIB_CALLCC_H
