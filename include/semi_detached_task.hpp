#ifndef MYLIB_SEMI_DETACHED_TASK_H
#define MYLIB_SEMI_DETACHED_TASK_H 1

#include <coroutine>
#include <type_traits>

#include "detached_task.hpp"
#include "symmetric_task_storage.hpp"

namespace mylib {

    template<typename T>
    struct fork_return
    {
        using type = T;
        T&& value;
    };

    template<typename Void>
        requires (std::is_void_v<Void>)
    struct fork_return<Void>
    {
        using type = Void;
    };

    template<typename T>
    fork_return(T&&) -> fork_return<T>;

    fork_return() -> fork_return<void>;

    namespace details {

        template<typename TaskType>
        class semi_detached_task_promise : public mylib::details::detached_task_promise<TaskType>
        {
        public:
            using task_type = TaskType;
            using promise_type = semi_detached_task_promise;
            using handle_type = std::coroutine_handle<promise_type>;
            using return_type = task_type::return_type;
            using detached_base = mylib::details::detached_task_promise<task_type>;
            using symmetric_storage = mylib::symmetric_task_storage<return_type>;
            
            task_type get_return_object() noexcept {
                return task_type(handle_type::from_promise(*this));
            }

            return_type do_resume() { return storage.do_resume(); }

            struct fork_return_awaiter : std::suspend_always
            {
                template<typename Promise>
                std::coroutine_handle<> await_suspend(std::coroutine_handle<Promise> current) noexcept {
                    return static_cast<promise_type&>(current.promise()).continuation;
                }
            };

            void unhandled_exception() noexcept(false) {
                if (storage.uninitialized()) {
                    storage.unhandled_exception();
                } else {
                    detached_base::unhandled_exception();
                }
            }

            template<typename T>
            fork_return_awaiter await_transform(fork_return<T> fr) noexcept {
                if constexpr (std::is_void_v<T>) {
                    static_assert(std::is_void_v<return_type>);
                    storage.return_void();
                } else {
                    static_assert(std::is_convertible_v<T, return_type>);
                    storage.return_value(static_cast<T&&>(fr.value));
                }
                return {};
            }

            // Forwarding transform
            template<typename T>
            T&& await_transform(T&& awaitable) noexcept
            {
                return static_cast<T&&>(awaitable);
            }

            void set_continuation(std::coroutine_handle<> c) noexcept { continuation = c; }

        private:
            symmetric_storage storage;
            std::coroutine_handle<> continuation = std::noop_coroutine();
        };

    } // namespace mylib::details

    template<typename ReturnType>
    class [[nodiscard]] semi_detached_task
    {
    public:
        using return_type = ReturnType;
        using promise_type = details::semi_detached_task_promise<semi_detached_task>;
        friend promise_type;
        using handle_type = promise_type::handle_type;
    private:
        explicit semi_detached_task(handle_type handle) noexcept : handle(handle) {}
        semi_detached_task() = default;
    public:
        semi_detached_task(const semi_detached_task&) = delete;
        semi_detached_task& operator=(const semi_detached_task&) = delete;

        semi_detached_task(semi_detached_task&& other) noexcept
            : handle(std::exchange(other.handle, nullptr))
        {}

        semi_detached_task& operator=(semi_detached_task&& other) noexcept {
            semi_detached_task().swap(other);
            return *this;
        }

        ~semi_detached_task() { if (this->handle) { handle.destroy(); } }

        void swap(semi_detached_task& other) noexcept {
            if (this == std::addressof(other)) { return; }
            std::ranges::swap(this->handle, other.handle);
        }

        [[nodiscard]]
        handle_type to_handle() && noexcept { return std::exchange(this->handle, nullptr); }

        struct task_awaiter : std::suspend_always
        {
        public:
            using task_type = semi_detached_task;
            using handle_type = task_type::handle_type;
            using return_type = task_type::return_type;

            [[nodiscard]] bool await_ready() noexcept { return !this->coroutine; }

            template<typename PromiseType>
            handle_type await_suspend(std::coroutine_handle<PromiseType> current) noexcept {
                this->coroutine.promise().set_continuation(current);
                return this->coroutine;
            }

            return_type await_resume() { return this->coroutine.promise().do_resume(); }

        private:
            friend task_type;
            explicit task_awaiter(handle_type handle) noexcept : coroutine(handle) {}

            handle_type coroutine = nullptr;
        };

        task_awaiter operator co_await() && noexcept {
            return task_awaiter(std::exchange(this->handle, nullptr));
        }

    private:
        handle_type handle = nullptr;
    };

} // namespace mylib

#endif // MYLIB_SEMI_DETACHED_TASK_H
