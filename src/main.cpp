#include <print>
#include <stdexcept>

#include "task.hpp"
#include "detached_task.hpp"
#include "callcc.hpp"

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

int main() {
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
