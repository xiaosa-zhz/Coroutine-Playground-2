#ifndef MYLIB_DETACHED_TASK_H
#define MYLIB_DETACHED_TASK_H 1

#include <coroutine>
#include <exception>
#include <memory>
#include <utility>

#include <cassert>

namespace mylib {

    class [[nodiscard]] detached_task
    {
    private:
        struct detached_task_promise
        {
            using handle_type = std::coroutine_handle<detached_task_promise>;
            detached_task get_return_object() noexcept { return detached_task(handle_type::from_promise(*this)); }
            void return_void() const noexcept {}
            std::suspend_always initial_suspend() const noexcept { return {}; }
            std::suspend_never final_suspend() const noexcept { return {}; } // coroutine destroyed on final suspend

            void unhandled_exception() noexcept(false) {
                // propagate exception to caller, executor, or whatever
                throw detached_task_unhandled_exit_exception(handle_type::from_promise(*this));
            }
        };

        detached_task() = default;

        using handle_type = detached_task_promise::handle_type;
        explicit detached_task(handle_type handle) noexcept : handle(handle) {}

    public:
        using promise_type = detached_task_promise;

        detached_task(const detached_task&) = delete;
        detached_task& operator=(const detached_task&) = delete;

        detached_task(detached_task&& other) noexcept
            : handle(std::exchange(other.handle, nullptr))
        {}

        detached_task& operator=(detached_task&& other) noexcept {
            detached_task().swap(other);
            return *this;
        }

        ~detached_task() { if (this->handle) { handle.destroy(); } }

        void swap(detached_task& other) noexcept {
            if (this == std::addressof(other)) { return; }
            std::ranges::swap(this->handle, other.handle);
        }

        // thrown when detached task exits with unhandled exception
        // nested exception is the exception thrown by the detached task
        // responsible for destroying the coroutine
        class detached_task_unhandled_exit_exception : public std::exception, public std::nested_exception
        {
        public:
            friend promise_type;

            // satisfy standard exception requirements

            detached_task_unhandled_exit_exception() = default;
            detached_task_unhandled_exit_exception(const detached_task_unhandled_exit_exception&) = default;
            detached_task_unhandled_exit_exception(detached_task_unhandled_exit_exception&&) = default;
            detached_task_unhandled_exit_exception& operator=(const detached_task_unhandled_exit_exception&) = default;
            detached_task_unhandled_exit_exception& operator=(detached_task_unhandled_exit_exception&&) = default;

            ~detached_task_unhandled_exit_exception() override = default;

            const char* what() const noexcept override { return message; }

        private:
            constexpr static auto message = "Detached task exits with unhandled exception.";

            static void handle_destroyer(void* p) noexcept { handle_type::from_address(p).destroy(); }

            explicit detached_task_unhandled_exit_exception(handle_type handle)
                // implicitly catch current exception via default init std::nested_exception
                : handle_holder(handle.address(), &handle_destroyer)
            {}

            std::shared_ptr<void> handle_holder = nullptr;
        };

        // can only be called once
        // once called, detached_task object is not responsible for destroying the coroutine
        void start() && {
            assert(this->handle && "Task already start!");
            std::exchange(this->handle, nullptr).resume();
        }

        [[nodiscard]]
        handle_type to_handle() && noexcept { return std::exchange(this->handle, nullptr); }

    private:
        handle_type handle = nullptr;
    };

} // namespace mylib

#endif // MYLIB_DETACHED_TASK_H
