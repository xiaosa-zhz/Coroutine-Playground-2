#ifndef MYLIB_SYMMETRIC_TASK_PROMISE_H
#define MYLIB_SYMMETRIC_TASK_PROMISE_H 1

#include <concepts>
#include <cstddef>
#include <variant>
#include <exception>

#include <cassert>

namespace mylib {

    namespace details {

        struct symmetric_task_storage_base
        {
            constexpr static std::size_t empty = 0;
            constexpr static std::size_t value = 1;
            constexpr static std::size_t exception = 2;
            using empty_type = std::monostate;
            using exception_type = std::exception_ptr;
            template<typename DataType>
            using storage_type = std::variant<empty_type, DataType, exception_type>;

            void throw_if_exception(this auto&& self) {
                assert(self.storage.index() != empty && "Task result is uninitialized.");
                if (exception_type* ex = std::get_if<exception>(&self.storage)) {
                    std::rethrow_exception(*ex);
                }
            }

            void unhandled_exception(this auto&& self, exception_type e = std::current_exception()) noexcept {
                self.storage.template emplace<exception>(std::move(e));
            }
        };

    } // namespace mylib::details

    template<typename ReturnType>
    class symmetric_task_storage : public details::symmetric_task_storage_base
    {
    public:
        using return_type = ReturnType;
    private:
        using base = details::symmetric_task_storage_base;
        friend base;
        constexpr static bool return_reference = std::is_reference_v<return_type>;
    public:
        using data_type = std::conditional_t<return_reference, std::add_pointer_t<return_type>, return_type>;
        using storage_type = base::storage_type<data_type>;

        void return_value(return_type rt) noexcept requires (return_reference) {
            this->storage.template emplace<value>(std::addressof(rt));
        }

        template<typename U = return_type>
            requires (not return_reference) && std::convertible_to<U, return_type> && std::constructible_from<return_type, U>
        void return_value(U&& rt) noexcept(std::is_nothrow_constructible_v<return_type, U>) {
            this->storage.template emplace<value>(std::forward<U>(rt));
        }

        return_type do_resume() {
            throw_if_exception();
            if constexpr (return_reference) {
                return static_cast<return_type>(*std::get<value>(this->storage));
            } else {
                return std::move(std::get<value>(this->storage));
            }
        }

    private:
        storage_type storage;
    };

    template<typename Void>
        requires (std::is_void_v<Void>)
    class symmetric_task_storage<Void> : public details::symmetric_task_storage_base
    {
    private:
        using base = details::symmetric_task_storage_base;
        friend base;
    public:
        using return_type = void;
        using data_type = std::monostate;
        using storage_type = base::storage_type<data_type>;

        void return_void() noexcept { this->storage.template emplace<value>(); }

        void do_resume() { throw_if_exception(); }

    private:
        storage_type storage;
    };

} // namespace mylib

#endif // MYLIB_SYMMETRIC_TASK_PROMISE_H
