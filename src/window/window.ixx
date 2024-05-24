module;

#include "ecs/ecs.hpp"
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <Windows.h>
#include <cstdint>
#include <expected>
#include <string>

export module stellar.window;

LRESULT CALLBACK wnd_proc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);

export struct Window {
    HWND hwnd;
    HINSTANCE hinstance;
    uint32_t width;
    uint32_t height;

    Window() = default;
    Window(const Window&) = delete;
    Window& operator=(const Window&) = delete;
    Window(Window&&) = default;
    Window& operator=(Window&&) = default;

    std::expected<void, std::string> initialize(uint32_t width_, uint32_t height_) {
        width = width_;
        height = height_;

        hinstance = GetModuleHandle(nullptr);
        const WNDCLASSEX wc{
            .cbSize = sizeof(WNDCLASSEX),
            .style = CS_HREDRAW | CS_VREDRAW,
            .lpfnWndProc = wnd_proc,
            .hInstance = hinstance,
            .hCursor = LoadCursor(nullptr, IDC_ARROW),
            .lpszClassName = "Stellar Engine",
        };
        RegisterClassEx(&wc);

        RECT window_rect = {.left = 0, .top = 0, .right = static_cast<LONG>(width), .bottom = static_cast<LONG>(height)};
        AdjustWindowRect(&window_rect, WS_OVERLAPPEDWINDOW, FALSE);

        hwnd = CreateWindow(
            wc.lpszClassName,
            "Stellar Engine",
            WS_OVERLAPPEDWINDOW,
            CW_USEDEFAULT,
            CW_USEDEFAULT,
            window_rect.right - window_rect.left,
            window_rect.bottom - window_rect.top,
            nullptr,
            nullptr,
            hinstance,
            this
        );
        if (hwnd == nullptr) {
            return std::unexpected("Failed to create window");
        }

        ShowWindow(hwnd, SW_SHOW);
        return {};
    }
};

void poll_window(flecs::iter& it) {
    while (it.next()) {}
    MSG msg;
    while (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
        if (msg.message == WM_QUIT) {
            it.world().quit();
        }
    }
}

export std::expected<void, std::string> initialize_window(const flecs::world& world, const uint32_t width, const uint32_t height) {
    Window window{};
    if (const auto res = window.initialize(width, height); !res.has_value()) {
        return res;
    }

    world.set(window);
    world.system<Window>("Window Events")
        .term_at(0).singleton()
        .kind(flecs::PreUpdate)
        .run(poll_window);

    return {};
}

LRESULT CALLBACK wnd_proc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    auto* window = reinterpret_cast<Window*>(GetWindowLongPtr(hwnd, GWLP_USERDATA));

    switch (msg) {
    case WM_CREATE: {
        auto create_struct = reinterpret_cast<LPCREATESTRUCT>(lParam);
        SetWindowLongPtr(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(create_struct->lpCreateParams));
        return 0;
    }
    case WM_DESTROY: {
        PostQuitMessage(0);
        return 0;
    }
    default: {
        return DefWindowProc(hwnd, msg, wParam, lParam);
    }
    }
}