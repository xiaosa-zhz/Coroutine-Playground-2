#include <coroutine>
#include <print>
#include <stdexcept>

#include "semi_detached_task.hpp"
#include "task.hpp"
#include "detached_task.hpp"
#include "callcc.hpp"
#include "transaction.hpp"

mylib::task<int> work() {
    std::println("Work, work");
    co_return 0;
}

mylib::task<void> ex() {
    std::println("Work result: {}", co_await work());
    std::println("Throwing exception");
    throw std::runtime_error("Exception ex");
    std::unreachable();
}

mylib::task<int> a_main() {
    try {
        co_await ex();
    }
    catch (std::exception& e) {
        std::println("Exception happened: \"{}\"", e.what());
    }
    co_return 0;
}

struct noizy {
    noizy() noexcept { std::println("Noizy default construction."); }
    noizy(const noizy&) noexcept { std::println("Noizy copy construction."); }
    noizy(noizy&&) noexcept { std::println("Noizy move construction."); }
    ~noizy() { std::println("Noizy destruction."); }
};

mylib::detached_task test([[maybe_unused]] noizy _ = {}) {
    throw std::runtime_error("Exception test");
    co_return; // make it coroutine
}

mylib::detached_task func() {
    auto result = co_await []() -> mylib::callcc_task<int> {
        co_await [](auto cc) -> mylib::task<void> {
            co_await [](auto cc) -> mylib::task<void> {
                co_await cc(42);
                std::println("Never");
            }(cc);
            std::println("Never");
        }(co_await mylib::get_cc());
        std::println("Never");
        co_return 114514;
    }();
    std::println("Answer is {}", result);
}

mylib::detached_task func_ex() {
    try {
        co_await []() -> mylib::callcc_task<void> {
            co_await [](mylib::cc<void> cc) -> mylib::task<void> {
                std::exception_ptr p;
                try {
                    co_await ex();
                }
                catch (std::exception&) {
                    p = std::current_exception();
                }
                co_await cc.call_with_exception(p);
                std::println("Never");
            }(co_await mylib::get_cc());
            std::println("Never");
        }();
    }
    catch (std::exception& e) {
        std::println("cc exception: \"{}\"", e.what());
    }
}

struct get_handle_awaiter : std::suspend_always
{
    template<typename PromiseType>
    bool await_suspend(std::coroutine_handle<PromiseType> h) noexcept {
        handle = h;
        return false;
    }

    std::coroutine_handle<> handle = nullptr;
};

mylib::semi_detached_task<int> fork_func(get_handle_awaiter& a) {
    noizy _{};
    co_await a;
    co_await mylib::fork_return(42);
    std::println("Fork part");
    // co_return;
}

mylib::detached_task test_fork(get_handle_awaiter& a) {
    auto t = co_await fork_func(a);
    std::println("Forked task result: {}", t);
}

struct fake_database
{
    mylib::task<void> transaction_begin() noexcept {
        std::println("DB: Transaction begin");
        co_return;
    }

    mylib::task<void> transaction_commit() noexcept {
        std::println("DB: Transaction commit");
        co_return;
    }

    mylib::task<void> transaction_rollback() noexcept {
        std::println("DB: Transaction rollback");
        co_return;
    }

    mylib::task<void> do_something() noexcept {
        std::println("DB: Doing something");
        co_return;
    }
};

mylib::transaction<int> test_commit(fake_database& db) {
    co_await db.do_something();
    co_return 42;
}

mylib::transaction<int> test_rollback(fake_database& db) {
    co_await db.do_something();
    throw std::runtime_error("Error happened");
}

mylib::detached_task test_transaction() {
    fake_database db{};
    auto result = co_await test_commit(db);
    std::println("Transaction committed with result: {}", result);
    try {
        co_await test_rollback(db);
    } catch (std::exception& e) {
        std::println("Transaction rolled back with exception: \"{}\"", e.what());
    }
}

int main() {
    test_transaction().start();
    get_handle_awaiter a;
    test_fork(a).start();
    std::println("Resuming");
    a.handle.resume();
    func().start();
    func_ex().start();
    try {
        test().start();
    }
    catch (std::exception& e) {
        std::println("Exception happened: \"{}\"", e.what());
        try {
            std::rethrow_if_nested(e);
        } catch (std::exception& ne) {
            std::println("Nested exception: \"{}\"", ne.what());
        }
    }
    return a_main().sync_await();
}
