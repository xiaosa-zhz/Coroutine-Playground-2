#pragma once
#ifndef MYLIB_TRANSACTION_H
#define MYLIB_TRANSACTION_H 1

#include <coroutine>
#include <utility>
#include <concepts>
#include <type_traits>
#include <memory>

#include "task.hpp"
#include "detached_task.hpp"

namespace mylib {

    namespace details {
    
        template<typename Arg>
        concept member_begin = requires (Arg&& arg) { std::forward<Arg>(arg).transaction_begin(); };

        template<typename Arg>
        concept adl_begin = requires (Arg&& arg) { transaction_begin(std::forward<Arg>(arg)); };

        template<typename Arg>
        concept has_begin = member_begin<Arg> || adl_begin<Arg>;

        template<typename Arg>
        concept nothrow_begin =
            member_begin<Arg>
                && requires (Arg&& arg) { { std::forward<Arg>(arg).transaction_begin() } noexcept; }
            || adl_begin<Arg>
                && requires (Arg&& arg) { { transaction_begin(std::forward<Arg>(arg)) } noexcept; };

        template<typename Arg>
        struct begin_traits {};

        template<member_begin Arg>
        struct begin_traits<Arg> {
            using type = decltype(std::declval<Arg>().transaction_begin());
        };

        template<adl_begin Arg>
        struct begin_traits<Arg> {
            using type = decltype(transaction_begin(std::declval<Arg>()));
        };

        template<has_begin Arg>
        using begin_type = begin_traits<Arg>::type;

        template<typename Arg>
        concept member_commit = requires (Arg&& arg) { std::forward<Arg>(arg).transaction_commit(); };

        template<typename Arg>
        concept adl_commit = requires (Arg&& arg) { transaction_commit(std::forward<Arg>(arg)); };

        template<typename Arg>
        concept has_commit = member_commit<Arg> || adl_commit<Arg>;

        template<typename Arg>
        concept nothrow_commit =
            member_commit<Arg>
                && requires (Arg&& arg) { { std::forward<Arg>(arg).transaction_commit() } noexcept; }
            || adl_commit<Arg>
                && requires (Arg&& arg) { { transaction_commit(std::forward<Arg>(arg)) } noexcept; };

        template<typename Arg>
        struct commit_traits {};

        template<member_commit Arg>
        struct commit_traits<Arg> {
            using type = decltype(std::declval<Arg>().transaction_commit());
        };

        template<adl_commit Arg>
        struct commit_traits<Arg> {
            using type = decltype(transaction_commit(std::declval<Arg>()));
        };

        template<has_commit Arg>
        using commit_type = commit_traits<Arg>::type;

        template<typename Arg>
        concept member_rollback = requires (Arg&& arg) { std::forward<Arg>(arg).transaction_rollback(); };

        template<typename Arg>
        concept adl_rollback = requires (Arg&& arg) { transaction_rollback(std::forward<Arg>(arg)); };

        template<typename Arg>
        concept has_rollback = member_rollback<Arg> || adl_rollback<Arg>;

        template<typename Arg>
        concept nothrow_rollback =
            member_rollback<Arg>
                && requires (Arg&& arg) { { std::forward<Arg>(arg).transaction_rollback() } noexcept; }
            || adl_rollback<Arg>
                && requires (Arg&& arg) { { transaction_rollback(std::forward<Arg>(arg)) } noexcept; };

        template<typename Arg>
        struct rollback_traits {};

        template<member_rollback Arg>
        struct rollback_traits<Arg> {
            using type = decltype(std::declval<Arg>().transaction_rollback());
        };

        template<adl_rollback Arg>
        struct rollback_traits<Arg> {
            using type = decltype(transaction_rollback(std::declval<Arg>()));
        };

        template<has_rollback Arg>
        using rollback_type = rollback_traits<Arg>::type;

        template<typename>
        inline constexpr bool always_false = false;

        struct transaction_begin_cpo {
            template<details::has_begin Arg>
            details::begin_type<Arg> operator() (Arg&& arg) const noexcept(details::nothrow_begin<Arg>) {
                if constexpr (requires { std::forward<Arg>(arg).transaction_begin(); }) {
                    return std::forward<Arg>(arg).transaction_begin();
                } else if constexpr (requires { transaction_begin(std::forward<Arg>(arg)); }) {
                    return transaction_begin(std::forward<Arg>(arg));
                } else {
                    static_assert(details::always_false<Arg>, "transaction_begin not found");
                }
            }
        };

        struct transaction_commit_cpo {
            template<details::has_commit Arg>
            details::commit_type<Arg> operator() (Arg&& arg) const noexcept(details::nothrow_commit<Arg>) {
                if constexpr (requires { std::forward<Arg>(arg).transaction_commit(); }) {
                    return std::forward<Arg>(arg).transaction_commit();
                } else if constexpr (requires { transaction_commit(std::forward<Arg>(arg)); }) {
                    return transaction_commit(std::forward<Arg>(arg));
                } else {
                    static_assert(details::always_false<Arg>, "transaction_commit not found");
                }
            }
        };

        struct transaction_rollback_cpo {
            template<details::has_rollback Arg>
            details::rollback_type<Arg> operator() (Arg&& arg) const noexcept(details::nothrow_rollback<Arg>) {
                if constexpr (requires { std::forward<Arg>(arg).transaction_rollback(); }) {
                    return std::forward<Arg>(arg).transaction_rollback();
                } else if constexpr (requires { transaction_rollback(std::forward<Arg>(arg)); }) {
                    return transaction_rollback(std::forward<Arg>(arg));
                } else {
                    static_assert(details::always_false<Arg>, "transaction_rollback not found");
                }
            }
        };

    } // namespace mylib::details

    inline constexpr details::transaction_begin_cpo transaction_begin{};
    inline constexpr details::transaction_commit_cpo transaction_commit{};
    inline constexpr details::transaction_rollback_cpo transaction_rollback{};

    // Forward declaration
    template<typename ReturnType>
    class transaction;

    namespace details {

        template<typename Awaitable>
        concept member_co_await = requires (Awaitable&& a) { std::forward<Awaitable>(a).operator co_await(); };

        template<typename Awaitable>
        concept adl_co_await = requires (Awaitable&& a) { operator co_await(std::forward<Awaitable>(a)); };

        template<typename Awaitable>
        struct awaiter_traits {
            using type = Awaitable;
        };

        template<member_co_await Awaitable>
        struct awaiter_traits<Awaitable> {
            using type = decltype(std::declval<Awaitable>().operator co_await());
        };

        template<adl_co_await Awaitable>
        struct awaiter_traits<Awaitable> {
            using type = decltype(operator co_await(std::declval<Awaitable>()));
        };

        template<typename Awaitable>
        using awaiter_type = awaiter_traits<Awaitable>::type;

        template<typename Awaitable>
        concept nothrow_co_await = [] {
            if constexpr (member_co_await<Awaitable>) {
                return requires (Awaitable&& a) { { std::forward<Awaitable>(a).operator co_await() } noexcept; };
            } else if constexpr (adl_co_await<Awaitable>) {
                return requires (Awaitable&& a) { { operator co_await(std::forward<Awaitable>(a)) } noexcept; };
            } else {
                return true;
            }
        }();

        template<typename Awaitable>
        awaiter_type<Awaitable> get_awaiter(Awaitable&& a) noexcept(nothrow_co_await<Awaitable>) {
            if constexpr (requires { std::forward<Awaitable>(a).operator co_await(); }) {
                return std::forward<Awaitable>(a).operator co_await();
            } else if constexpr (requires { operator co_await(std::forward<Awaitable>(a)); }) {
                return operator co_await(std::forward<Awaitable>(a));
            } else {
                return std::forward<Awaitable>(a);
            }
        }

        template<typename Awaitable>
        using await_result_t = decltype(std::declval<awaiter_type<Awaitable>>().await_resume());

        template<typename Arg>
        inline mylib::task<await_result_t<begin_type<Arg>>> begin_task(Arg& arg) {
            co_return co_await mylib::transaction_begin(std::forward<Arg>(arg));
        }

        template<typename Arg>
        inline mylib::task<void> commit_task(Arg& arg) {
            co_await mylib::transaction_commit(std::forward<Arg>(arg));
        }

        template<typename Arg>
        inline mylib::task<void> rollback_task(Arg& arg) {
            co_await mylib::transaction_rollback(std::forward<Arg>(arg));
        }

        template<typename Arg>
        inline mylib::detached_task detached_rollback_task(Arg& arg) {
            co_await mylib::transaction_rollback(std::forward<Arg>(arg));
        }

        inline mylib::task<void> noop_task() noexcept {
            co_return;
        }

        template<typename ReturnType>
        struct transaction_promise_base
        {
            using return_type = ReturnType;
            virtual std::coroutine_handle<> from_promise() noexcept = 0;
            virtual std::coroutine_handle<> transaction_suspend(std::coroutine_handle<>) noexcept = 0;
            virtual return_type do_resume() = 0;
        };

        template<typename ReturnType>
        struct promise_deleter {
            static void operator() (transaction_promise_base<ReturnType>* p) noexcept {
                p->from_promise().destroy();
            }
        };

        template<typename ReturnType>
        using transaction_handle_type = std::unique_ptr<transaction_promise_base<ReturnType>, promise_deleter<ReturnType>>;

        // Forward declaration
        template<typename ReturnType, typename Arg>
        struct transaction_promise;

        template<typename ReturnType>
        class [[nodiscard]] transaction_awaiter
        {
        public:
            using return_type = ReturnType;
            using handle_type = transaction_handle_type<return_type>;

            transaction_awaiter(const transaction_awaiter&) = delete;
            transaction_awaiter& operator=(const transaction_awaiter&) = delete;

            void swap(transaction_awaiter& other) noexcept {
                std::ranges::swap(this->handle, other.handle);
            }

            transaction_awaiter(transaction_awaiter&& other) noexcept
                : handle(std::exchange(other.handle, nullptr))
            {}

            transaction_awaiter& operator=(transaction_awaiter&& other) noexcept {
                transaction_awaiter().swap(other);
                return *this;
            }

            ~transaction_awaiter() = default;

            [[nodiscard]] bool await_ready() noexcept { return !this->handle; }

            std::coroutine_handle<> await_suspend(std::coroutine_handle<> current) noexcept {
                return this->handle->transaction_suspend(current);
            }

            return_type await_resume() {
                return this->handle->do_resume();
            }

        private:
            template<typename>
            friend class mylib::transaction;

            explicit transaction_awaiter(handle_type&& handle) noexcept
                : handle(std::move(handle))
            {}

            transaction_awaiter() = default;

            handle_type handle = nullptr;
        };

    } // namespace mylib::details

    template<typename ReturnType>
    class transaction
    {
    public:
        using return_type = ReturnType;
        using awaiter_type = details::transaction_awaiter<return_type>;

        transaction(const transaction&) = delete;
        transaction& operator=(const transaction&) = delete;

        void swap(transaction& other) noexcept {
            std::ranges::swap(this->handle, other.handle);
        }

        transaction(transaction&& other) noexcept
            : handle(std::exchange(other.handle, nullptr))
        {}

        transaction& operator=(transaction&& other) noexcept {
            transaction().swap(other);
            return *this;
        }

        ~transaction() = default;

        awaiter_type operator co_await() && noexcept {
            return awaiter_type(std::move(this->handle));
        }

    private:
        using handle_type = details::transaction_handle_type<return_type>;

        template<typename, typename>
        friend struct details::transaction_promise;

        explicit transaction(handle_type&& handle) noexcept
            : handle(std::move(handle))
        {}

        transaction() = default;

        handle_type handle = nullptr;
    };

    struct get_begin_result_tag {};

    consteval get_begin_result_tag begin_result() { return {}; }

    struct eager_rollback_tag {};

    consteval eager_rollback_tag eager_rollback() { return {}; }

    namespace details {

        enum class transaction_status {
            done, need_rollback, need_commit
        };

        template<typename ReturnType>
        struct return_base
        {
            using return_type = ReturnType;
            constexpr static bool return_reference = std::is_reference_v<return_type>;
            
            template<typename Self>
            void return_value(this Self& self, return_type rt) noexcept requires (return_reference) {
                if (self.status != transaction_status::done) {
                    self.status = transaction_status::need_commit;
                }
                self.storage.return_value(rt);
            }

            template<typename Self, typename U = return_type>
                requires (not return_reference) && std::convertible_to<U, return_type> && std::constructible_from<return_type, U>
            void return_value(this Self& self, U&& rt) noexcept(std::is_nothrow_constructible_v<return_type, U>) {
                if (self.status != transaction_status::done) {
                    self.status = transaction_status::need_commit;
                }
                self.storage.return_value(std::forward<U>(rt));
            }
        };

        template<typename Void>
            requires (std::is_void_v<Void>)
        struct return_base<Void>
        {
            template<typename Self>
            void return_void(this Self& self) noexcept {
                if (self.status != transaction_status::done) {
                    self.status = transaction_status::need_commit;
                }
                self.storage.return_void();
            }
        };

        template<typename ReturnType, typename Arg>
        struct transaction_promise : transaction_promise_base<ReturnType>, return_base<ReturnType>
        {
            using return_type = ReturnType;
            using transaction_type = transaction<return_type>;
            using promise_type = transaction_promise;
            using handle_type = std::coroutine_handle<promise_type>;

            friend return_base<return_type>;

            template<typename... Rests>
            transaction_promise(Arg& arg, Rests&&...) noexcept
                : first_arg(arg)
            {}

            std::coroutine_handle<> from_promise() noexcept override {
                return handle_type::from_promise(*this);
            }

            std::coroutine_handle<> transaction_suspend(std::coroutine_handle<> h) noexcept override {
                this->continuation = h;
                return this->begin_task_handle;
            }

            return_type do_resume() override { return this->storage.do_resume(); }

            transaction_type get_return_object() noexcept {
                return transaction_type{ transaction_handle_type<return_type>(this) };
            }

            using begin_result_type = await_result_t<begin_type<Arg>>;
            using begin_task_type = mylib::task<begin_result_type>;
            using begin_task_awaiter_type = begin_task_type::task_awaiter;
            using begin_task_handle_type = begin_task_type::handle_type;
            using begin_result_storage_type = std::conditional_t<
                std::is_void_v<begin_result_type>,
                std::monostate,
                std::variant<std::monostate, begin_result_type>
            >;

            struct transaction_initial_awaiter
            {
                begin_task_awaiter_type awaiter;
                promise_type* promise;

                constexpr bool await_ready() const noexcept { return false; }

                void await_suspend(handle_type current) noexcept {
                    promise->begin_task_handle = awaiter.await_suspend(current);
                }

                constexpr void await_resume() noexcept {
                    if constexpr (!std::is_void_v<begin_result_type>) {
                        promise->begin_result.template emplace<1>(awaiter.await_resume());
                    }
                }
            };

            transaction_initial_awaiter initial_suspend() {
                return { begin_task(first_arg).operator co_await(), this };
            }

            using final_task_type = mylib::task<void>;
            using final_task_awaiter_type = final_task_type::task_awaiter;

            struct transaction_final_awaiter
            {
                final_task_awaiter_type awaiter;
                promise_type* promise;

                constexpr bool await_ready() const noexcept { return false; }

                std::coroutine_handle<> await_suspend(handle_type current) noexcept {
                    return awaiter.await_suspend(promise->continuation);
                }

                constexpr void await_resume() const noexcept {}
            };

            transaction_final_awaiter final_suspend() noexcept {
                switch (status) {
                    case transaction_status::need_rollback:
                        status = transaction_status::done;
                        return { rollback_task(first_arg).operator co_await(), this };
                    case transaction_status::need_commit:
                        status = transaction_status::done;
                        return { commit_task(first_arg).operator co_await(), this };
                    case transaction_status::done:
                        return { noop_task().operator co_await(), this };
                    default:
                        std::unreachable();
                }
            }

            void unhandled_exception() noexcept { storage.unhandled_exception(); }

            ~transaction_promise() {
                switch (status) {
                    case transaction_status::need_rollback:
                        try {
                            detached_rollback_task(first_arg).start();
                        } catch (...) {
                            // swallow all exceptions
                        }
                        [[fallthrough]];
                    case transaction_status::done:
                        return;
                    case transaction_status::need_commit:
                    default:
                        std::unreachable();
                }
            }

            // Forwarding overload
            template<typename T>
            T&& await_transform(T&& value) noexcept { return std::forward<T>(value); }

            struct begin_result_awaiter : std::suspend_never
            {
                begin_result_storage_type* begin_result;

                begin_result_type await_resume()
                    noexcept(std::is_void_v<begin_result_type> || std::is_nothrow_move_constructible_v<begin_result_type>) {
                    if constexpr (std::is_void_v<begin_result_type>) {
                        return;
                    } else {
                        return std::move(*std::get_if<1>(begin_result));
                    }
                }
            };

            begin_result_awaiter await_transform(mylib::get_begin_result_tag) noexcept {
                return begin_result_awaiter{ {}, &this->begin_result };
            }

            struct eager_rollback_awaiter
            {
                final_task_awaiter_type awaiter;
                promise_type* promise;

                bool await_ready() const noexcept {
                    if (promise->status == transaction_status::done) {
                        return true;
                    }
                    return false;
                }

                std::coroutine_handle<> await_suspend(handle_type current) noexcept {
                    promise->status = transaction_status::done;
                    return awaiter.await_suspend(current);
                }

                constexpr void await_resume() const noexcept {}
            };

            eager_rollback_awaiter await_transform(mylib::eager_rollback_tag) noexcept {
                return { rollback_task(first_arg).operator co_await(), this };
            }

        private:
            Arg& first_arg;
            begin_task_handle_type begin_task_handle = nullptr;
            begin_result_storage_type begin_result;
            std::coroutine_handle<> continuation = std::noop_coroutine();
            mylib::symmetric_task_storage<return_type> storage;
            transaction_status status = transaction_status::need_rollback;
        };

    } // namespace mylib::details

} // namespace mylib

template<typename ReturnType, typename First, typename... Rests>
struct std::coroutine_traits<mylib::transaction<ReturnType>, First, Rests...>
{
    using promise_type = mylib::details::transaction_promise<ReturnType, std::remove_cvref_t<First>>;
};

#endif // MYLIB_TRANSACTION_H
