#include <print>
#include <stdexcept>

#include "task.hpp"
#include "detached_task.hpp"

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

int main() {
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
