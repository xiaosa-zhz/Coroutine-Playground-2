#include <print>

#include "task.hpp"

mylib::task<int> bar() {
    co_return 42;
}

mylib::task<void> foo() {
    std::println("The answer is: {}", co_await bar());
}

int main() {
    foo().sync_await();
}
