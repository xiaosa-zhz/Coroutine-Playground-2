#ifndef MYLIB_COROUTINE_TASK_H
#define MYLIB_COROUTINE_TASK_H 1

#include <coroutine>
#include <utility>
#include <memory>

#include "symmetric_task_storage.hpp"

namespace mylib {

    namespace details {
    
        template<typename TaskType>
        class task_promise : public mylib::symmetric_task_storage<typename TaskType::return_type>
        {
        public:
            using task_type = TaskType;
            using handle_type = std::coroutine_handle<task_promise>;
            using return_type = typename task_type::return_type;

            // inherited from symmetric_task_storage:
            // unhandled_exception
            // return_value or return_void
            // do_resume

            struct [[nodiscard]] final_awaiter
            {
                bool await_ready() const noexcept { return false; }

                template<typename PromiseType>
                std::coroutine_handle<> await_suspend(std::coroutine_handle<PromiseType> current_coroutine) noexcept {
                    return static_cast<task_promise&>(current_coroutine.promise()).continuation;
                }

                void await_resume() const noexcept { std::unreachable(); }
            };

            task_type get_return_object() { return task_type(handle_type::from_promise(*this)); }
            std::suspend_always initial_suspend() noexcept { return {}; }
            final_awaiter final_suspend() noexcept { return {}; }

            std::suspend_always yield_value() { return {}; }

            void set_continuation(std::coroutine_handle<> c) noexcept { continuation = c; }

        protected:
            std::coroutine_handle<> continuation = std::noop_coroutine();
        };

        template<typename TaskType>
        class [[nodiscard]] task_awaiter
        {
        public:
            using task_type = TaskType;
            using handle_type = typename task_type::handle_type;
            using return_type = typename task_type::return_type;

            task_awaiter(const task_awaiter&) = delete;
            task_awaiter& operator=(const task_awaiter&) = delete;

            void swap(task_awaiter& other) noexcept {
                std::ranges::swap(this->coroutine, other.coroutine);
            }

            task_awaiter(task_awaiter&& other) noexcept
                : coroutine(std::exchange(other.coroutine, nullptr))
            {}

            task_awaiter& operator=(task_awaiter&& other) noexcept {
                task_awaiter().swap(other);
                return *this;
            }

            ~task_awaiter() { if (this->coroutine) { this->coroutine.destroy(); } }

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

            task_awaiter() = default;

            handle_type coroutine = nullptr;
        };

    } // namespace mylib::details

    template<typename ReturnType>
    class [[nodiscard]] task
    {
    public:
        using return_type = ReturnType;
        using promise_type = details::task_promise<task>;
        using handle_type = typename promise_type::handle_type;
        using task_awaiter = details::task_awaiter<task>;

        task(const task&) = delete;
        task& operator=(const task&) = delete;

        task(task&& other) noexcept : coroutine(std::exchange(other.coroutine, nullptr)) {}
        task& operator=(task&& other) noexcept {
            task().swap(other);
            return *this;
        }

        void swap(task& other) noexcept {
            if (this == std::addressof(other)) return;
            std::ranges::swap(this->coroutine, other.coroutine);
        }

        ~task() { if (this->coroutine) { this->coroutine.destroy(); } }

        task_awaiter operator co_await() && noexcept {
            return task_awaiter(std::exchange(this->coroutine, nullptr));
        }

        // Test only
        return_type sync_await() && {
            task_awaiter awaiter = std::move(*this).operator co_await();
            awaiter.await_suspend(std::noop_coroutine()).resume();
            return awaiter.await_resume();
        }

    private:
        friend promise_type;
        task() = default;
        explicit task(handle_type handle) noexcept : coroutine(handle) {}

        handle_type coroutine = nullptr;
    };

} // namespace mylib

#endif // MYLIB_COROUTINE_TASK_H
