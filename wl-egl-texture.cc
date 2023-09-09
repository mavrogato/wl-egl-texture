
#include <iostream>
#include <concepts>
#include <memory>
#include <functional>

#include <EGL/egl.h>
#include <GLES3/gl32.h>
#include <GLES2/gl2ext.h>
#include <wayland-client.h>

#include "xdg-wm-base-client.h"

#include "aux/io.hh"
#include "aux/unique_handle.hh"
#include "aux/tuple-support.hh"

namespace aux
{
    // namespace impl
    // {
    //     template <size_t N> struct to_tuple;

    //     template <> struct to_tuple<1> {
    //         template <class S> constexpr auto operator()(S&& s) const noexcept {
    //             auto [a] = std::forward<S>(s);
    //             return std::make_tuple(a);
    //         }
    //     };
    //     template <> struct to_tuple<2> {
    //         template <class S> constexpr auto operator()(S&& s) const noexcept {
    //             auto [a, b] = std::forward<S>(s);
    //             return std::make_tuple(a, b);
    //         }
    //     };
    //     template <> struct to_tuple<3> {
    //         template <class S> constexpr auto operator()(S&& s) const noexcept {
    //             auto [a, b, c] = std::forward<S>(s);
    //             return std::make_tuple(a, b, c);
    //         }
    //     };

    // } // ::impl

    // template <size_t N, class S>
    // auto to_tuple(S&& s) noexcept {
    //     return impl::to_tuple<N>{}(std::forward<S>(s));
    // }

    namespace impl
    {
        template <size_t N> struct to_function_tuple;

        template <> struct to_function_tuple<1> {
            template <class S>
            constexpr auto operator()(S&& s) const noexcept {
                auto [a] = std::forward<S>(s);
                return std::tuple{std::function{a}};
            }
        };
        template <> struct to_function_tuple<2> {
            template <class S>
            constexpr auto operator()(S&& s) const noexcept {
                auto [a, b] = std::forward<S>(s);
                return std::tuple{std::function{a}, std::function{b}};
            }
        };
    } // ::impl

    template <class S>
    constexpr auto to_function_tuple(S&& s) {
        return impl::to_function_tuple<sizeof (s) / sizeof (void*)>{}(std::forward<S>(s));
    }

} // ::aux

namespace wayland
{
    template <class> wl_interface const* const interface_ptr = nullptr;
    template <class T> concept client_like = (interface_ptr<T> != nullptr);
    template <client_like T> auto name = interface_ptr<T>->name;
    template <client_like T> void (*deleter)(T*);
    template <client_like T> struct listener {};

    template <> constexpr wl_interface const* const interface_ptr<wl_display> = &wl_display_interface;
    template <> void (*deleter<wl_display>)(wl_display*) = wl_display_disconnect;

#define INTERN_CLIENT_LIKE_2(CLIENT)                                    \
    template <> constexpr wl_interface const* const interface_ptr<CLIENT> = &CLIENT##_interface; \
    template <> void (*deleter<CLIENT>)(CLIENT*) = CLIENT##_destroy;

    INTERN_CLIENT_LIKE_2(wl_compositor)

#undef INTERN_CLIENT_LIKE_2

#define INTERN_CLIENT_LIKE_3(CLIENT)                              \
    template <> constexpr wl_interface const* const interface_ptr<CLIENT> = &CLIENT##_interface; \
    template <> void (*deleter<CLIENT>)(CLIENT*) = CLIENT##_destroy;    \
    template <> struct listener<CLIENT> : CLIENT##_listener { using base_type = CLIENT##_listener; };

    INTERN_CLIENT_LIKE_3(wl_registry)
    INTERN_CLIENT_LIKE_3(wl_surface)
    INTERN_CLIENT_LIKE_3(wl_seat)
    INTERN_CLIENT_LIKE_3(wl_keyboard)
    INTERN_CLIENT_LIKE_3(wl_pointer)
    INTERN_CLIENT_LIKE_3(wl_touch)

    INTERN_CLIENT_LIKE_3(xdg_wm_base)

#undef INTERN_CLIENT_LIKE_3

    template <client_like T>
    class client {
    private:

    public:
        explicit client(T* src = nullptr) noexcept
            : ptr{src, deleter<T>}
            {
                static constexpr auto N = sizeof (wayland::listener<T>) / sizeof (void*);
                [this]<size_t... I>(std::index_sequence<I...>) noexcept {
                    (void) (int[]){(std::get<I>(this->functions) = [](auto...) {}, 0)...};
                    this->listener.reset(new wayland::listener<T>
                                         {
                                             ([](void* self, auto... rest) {
                                                 std::get<I>(reinterpret_cast<client*>(self)->functions)(self, rest...);
                                             })...
                                         });
                }(std::make_index_sequence<N>());

                wl_proxy_add_listener((wl_proxy*) ptr.get(), reinterpret_cast<void (**)(void)>(this->listener.get()), (void*) this);
            }

        operator T*() const noexcept { return this->ptr.get(); }
        explicit operator bool() const noexcept { return !this->ptr(); }

        auto& global() { return std::get<0>(functions); }

    private:
        std::unique_ptr<T, decltype (deleter<T>)> ptr;
        std::unique_ptr<listener<T>> listener;
        decltype (aux::to_function_tuple(wayland::listener<T>{})) functions;
    };


} // ::wayland

int main() {
    auto display = std::unique_ptr<wl_display, decltype (wayland::deleter<wl_display>)>{
        wl_display_connect(nullptr),
        wayland::deleter<wl_display>,
    };
    auto registry = wayland::client<wl_registry>{wl_display_get_registry(display.get())};
    registry.global() = [&](auto... args) {
        std::cout << std::tuple{args...} <<std::endl;
    };
    wl_display_roundtrip(display.get());

    return 0;
}

#if 0
template <auto*> struct signature;

template <class Ret, class... Args, auto (*F)(Args...) -> Ret>
struct signature<F> {
    using result_type = Ret;
    using arguments_type = std::tuple<Args...>;
};

int main(int, char**) {
    wayland::client<wl_display> display{wl_display_connect(nullptr)};
    std::cout << display << std::endl;

    wl_registry_listener listener = {
        .global = [](auto...) { std::cout << "Hi" << std::endl; },
        .global_remove = [](auto...) {},
    };
    auto test = aux::to_tuple<2>(listener);

    assert(listener.global == std::get<0>(test));
    assert(listener.global_remove == std::get<1>(test));

    //std::cout << typeid (signature<std::declval<decltype (listener.global)>()>::result_type).name() << std::endl;
    std::cout << sizeof (std::declval<decltype (listener.global)>()) << std::endl;

    std::function f{std::get<0>(test)};

    f(nullptr, nullptr, 0, "", 0);

    return 0;
}
#endif


#if 0
namespace impl
{
    template <size_t N> struct to_function_tuple;

    template <> struct to_function_tuple<1> {
        template <class S>
        constexpr auto operator()(S&& s) const noexcept {
            auto [a] = std::forward<S>(s);
            return std::tuple{std::function{a}};
        }
    };
    template <> struct to_function_tuple<2> {
        template <class S>
        constexpr auto operator()(S&& s) const noexcept {
            auto [a, b] = std::forward<S>(s);
            return std::tuple{std::function{a}, std::function{b}};
        }
    };

} // ::impl

template <class S>
constexpr auto to_function_tuple(S&& s) {
    return impl::to_function_tuple<sizeof (s) / sizeof (void*)>{}(std::forward<S>(s));
}

int main() {
    wl_registry_listener listener;

    auto test = to_function_tuple(listener);

    std::cout << typeid (test).name() << std::endl;

    return 0;
}
#endif
