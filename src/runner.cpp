#include "epd_simulator.h"
#include <epd7in5_V2.h>
#define SDL_MAIN_HANDLED
#include <SDL.h>
#include <atomic>
#include <iostream>

namespace epd_simulator {
namespace {

struct WindowState {
    std::atomic<bool> quit{false};
    SDL_Window* window = nullptr;
};

// The watcher also sees events consumed inside Epd::DisplayFrame.
int WatchWindow(void* data, SDL_Event* event) {
    auto& state = *static_cast<WindowState*>(data);
    if (event->type == SDL_WINDOWEVENT && !state.window) {
        state.window = SDL_GetWindowFromID(event->window.windowID);
    }
    if (event->type == SDL_QUIT ||
        (event->type == SDL_WINDOWEVENT && event->window.event == SDL_WINDOWEVENT_CLOSE)) {
        state.quit.store(true);
    }

    return 1;
}

// Remove the watcher before its stack-owned state disappears.
struct WatchGuard {
    WindowState* state;

    ~WatchGuard() {
        SDL_DelEventWatch(WatchWindow, state);
        if (state->window) {
            SDL_HideWindow(state->window);
        }
    }
};

} // namespace

int Run(Epd& display, const char* title, const std::function<void()>& tick) {
    WindowState state;
    SDL_AddEventWatch(WatchWindow, &state);
    WatchGuard guard{&state};

    if (display.Init() != 0) {
        std::cerr << "Cannot initialise display: " << SDL_GetError() << '\n';
        return 1;
    }

    if (state.window && title) {
        SDL_SetWindowTitle(state.window, title);
    }

    // Keep network work in the caller's background worker, never in this loop.
    while (!state.quit.load()) {
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            // WatchWindow handles close events while the queue is drained.
        }
        if (state.quit.load()) {
            break;
        }

        tick();
        SDL_Delay(20);
    }

    return 0;
}

} // namespace epd_simulator
