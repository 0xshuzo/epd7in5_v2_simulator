#include "epd7in5_V2.h"
#include "epd_simulator.h"
#include "epd_simulator_config.h"

#define SDL_MAIN_HANDLED
#include <SDL.h>

#include <cerrno>
#include <cctype>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <optional>
#include <vector>

namespace {

constexpr double kMillimetresPerInch = 25.4;

std::optional<double> PositiveSetting(const char* name) {
    const char* value = std::getenv(name);
    if (!value || !*value) {
        return std::nullopt;
    }
    char* end = nullptr;
    errno = 0;
    const double parsed = std::strtod(value, &end);
    if (end == value || errno == ERANGE || !std::isfinite(parsed) || parsed <= 0) {
        return std::nullopt;
    }
    while (*end && std::isspace(static_cast<unsigned char>(*end))) {
        ++end;
    }
    return *end ? std::nullopt : std::optional<double>(parsed);
}

bool PhysicalPixels(double millimetres, double ppi, int& pixels) {
    const double value = millimetres * ppi / kMillimetresPerInch;
    if (!std::isfinite(millimetres) || millimetres <= 0 || !std::isfinite(ppi) || ppi <= 0 ||
        !std::isfinite(value) || value < 0.5 || value > std::numeric_limits<int>::max()) {
        SDL_SetError("Simulator window size is outside the supported pixel range");
        return false;
    }
    pixels = static_cast<int>(std::lround(value));
    return true;
}
} // namespace

struct Epd::Backend {
    SDL_Window* window = nullptr;
    SDL_Renderer* renderer = nullptr;
    SDL_Texture* texture = nullptr;
    std::vector<unsigned char> rgb_frame;
    bool closed = false;

    // One-shot callers still need close events to be handled after drawing.
    void PumpEvents() {
        SDL_Event event;
        while (SDL_PollEvent(&event) != 0) {
            if (event.type == SDL_QUIT) {
                closed = true;
            }
        }
    }

    // Both initialization and frame updates use the same nearest-pixel preview.
    void Present() {
        SDL_UpdateTexture(texture, nullptr, rgb_frame.data(), EPD_WIDTH * 3);
        SDL_RenderClear(renderer);
        SDL_RenderCopy(renderer, texture, nullptr, nullptr);
        SDL_RenderPresent(renderer);
    }

    ~Backend() {
        // Release SDL resources in the reverse order of their dependencies.
        if (texture != nullptr) {
            SDL_DestroyTexture(texture);
        }
        if (renderer != nullptr) {
            SDL_DestroyRenderer(renderer);
        }
        if (window != nullptr) {
            SDL_DestroyWindow(window);
        }
        SDL_QuitSubSystem(SDL_INIT_VIDEO);
    }
};

Epd::Epd() : backend_(nullptr) {
}
Epd::~Epd() {
    delete backend_;
}

int Epd::Init(void) {
    if (backend_ != nullptr) {
        return -1;
    }
    if (SDL_InitSubSystem(SDL_INIT_VIDEO) != 0) {
        return -1;
    }

    backend_ = new Backend();

    const double width_mm =
        PositiveSetting("EPD_SIMULATOR_WIDTH_MM").value_or(EPD_SIMULATOR_DEFAULT_WIDTH_MM);
    const double height_mm =
        PositiveSetting("EPD_SIMULATOR_HEIGHT_MM").value_or(EPD_SIMULATOR_DEFAULT_HEIGHT_MM);
    const auto configured_ppi = PositiveSetting("EPD_SIMULATOR_PPI");
    double ppi = configured_ppi.value_or(EPD_SIMULATOR_DEFAULT_PPI);
    int target_width = 0;
    int target_height = 0;
    if (!PhysicalPixels(width_mm, ppi, target_width) ||
        !PhysicalPixels(height_mm, ppi, target_height)) {
        return -1;
    }
    backend_->window = SDL_CreateWindow("7.5-inch e-Paper V2", SDL_WINDOWPOS_CENTERED,
        SDL_WINDOWPOS_CENTERED, target_width, target_height, SDL_WINDOW_ALLOW_HIGHDPI);
    if (backend_->window == nullptr) {
        return -1;
    }

    // Explicit calibration wins even when the monitor reports a different DPI.
    if (!configured_ppi) {
        const int display = SDL_GetWindowDisplayIndex(backend_->window);
        float display_dpi = 0.0F;
        if (display >= 0 && SDL_GetDisplayDPI(display, &display_dpi, nullptr, nullptr) == 0 &&
            std::isfinite(display_dpi) && display_dpi > 0) {
            ppi = display_dpi;
        }
    }
    if (!PhysicalPixels(width_mm, ppi, target_width) ||
        !PhysicalPixels(height_mm, ppi, target_height)) {
        return -1;
    }

    // Software rendering also supports machines without GPU acceleration.
    backend_->renderer = SDL_CreateRenderer(
        backend_->window, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    if (backend_->renderer == nullptr) {
        // Software rendering also supports machines without GPU acceleration.
        backend_->renderer = SDL_CreateRenderer(backend_->window, -1, SDL_RENDERER_SOFTWARE);
    }
    if (backend_->renderer == nullptr) {
        return -1;
    }
    SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "0");

    // Measure the current drawable-to-window ratio, then convert the desired
    // physical pixels to window coordinates. Using the desired size as the
    // denominator would apply a DPI change twice.
    int window_width = 0;
    int window_height = 0;
    int output_width = 0;
    int output_height = 0;
    SDL_GetWindowSize(backend_->window, &window_width, &window_height);
    if (SDL_GetRendererOutputSize(backend_->renderer, &output_width, &output_height) != 0 ||
        window_width <= 0 || window_height <= 0 || output_width <= 0 || output_height <= 0) {
        SDL_SetError("Cannot determine simulator window pixel scale");
        return -1;
    }
    const double logical_width = static_cast<double>(target_width) * window_width / output_width;
    const double logical_height =
        static_cast<double>(target_height) * window_height / output_height;
    if (logical_width < 0.5 || logical_height < 0.5 ||
        logical_width > std::numeric_limits<int>::max() ||
        logical_height > std::numeric_limits<int>::max()) {
        SDL_SetError("Simulator window size is outside the supported window coordinate range");
        return -1;
    }
    SDL_SetWindowSize(backend_->window, static_cast<int>(std::lround(logical_width)),
        static_cast<int>(std::lround(logical_height)));

    // Start with a white frame, matching an erased physical display.
    backend_->texture = SDL_CreateTexture(backend_->renderer, SDL_PIXELFORMAT_RGB24,
        SDL_TEXTUREACCESS_STREAMING, EPD_WIDTH, EPD_HEIGHT);
    if (backend_->texture == nullptr) {
        return -1;
    }
    backend_->rgb_frame.assign(EPD_WIDTH * EPD_HEIGHT * 3, 255);
    backend_->Present();
    return 0;
}

void Epd::WaitUntilIdle(void) {
    // Desktop presentation has no hardware BUSY signal to poll.
}

void Epd::Reset(void) {
    // The preview has no physical reset line.
}

void Epd::SetLut(void) {
    // SDL displays final pixels without simulating panel voltage waveforms.
}

void Epd::DisplayFrame(const unsigned char* frame_buffer) {
    if (frame_buffer == nullptr) {
        return;
    }
    if (backend_ == nullptr || backend_->closed) {
        return;
    }

    // Expand the packed monochrome pixels to the RGB texture SDL expects.
    for (unsigned long byte = 0; byte < EPD_FRAME_BYTES; ++byte) {
        const unsigned char source = frame_buffer[byte];
        for (unsigned char bit = 0; bit < 8; ++bit) {
            const unsigned char shade = (source & (0x80U >> bit)) == 0 ? 0 : 255;
            const size_t pixel = (byte * 8 + bit) * 3;
            backend_->rgb_frame[pixel] = shade;
            backend_->rgb_frame[pixel + 1] = shade;
            backend_->rgb_frame[pixel + 2] = shade;
        }
    }

    backend_->Present();
    backend_->PumpEvents();
}

void Epd::Displaypart(const unsigned char* pbuffer, unsigned long x_start, unsigned long y_start,
    unsigned long picture_width, unsigned long picture_height) {
    if (pbuffer == nullptr || x_start >= EPD_WIDTH || y_start >= EPD_HEIGHT || picture_width == 0 ||
        picture_height == 0 || x_start % 8 != 0 || picture_width % 8 != 0) {
        return;
    }

    // Limit the copied rectangle before computing its packed row offsets.
    if (picture_width > EPD_WIDTH - x_start) {
        picture_width = EPD_WIDTH - x_start;
    }
    if (picture_height > EPD_HEIGHT - y_start) {
        picture_height = EPD_HEIGHT - y_start;
    }

    // Waveshare's driver sends a white frame outside this image region.
    std::vector<unsigned char> full_frame(EPD_FRAME_BYTES, 0xFF);
    const unsigned long bytes_per_row = picture_width / 8;
    for (unsigned long y = 0; y < picture_height; ++y) {
        for (unsigned long x_byte = 0; x_byte < bytes_per_row; ++x_byte) {
            full_frame[(y_start + y) * (EPD_WIDTH / 8) + x_start / 8 + x_byte] =
                pbuffer[y * bytes_per_row + x_byte];
        }
    }

    DisplayFrame(full_frame.data());
}

void Epd::SendCommand(unsigned char command) {
    // Raw controller commands have no effect on the desktop preview.
    (void)command;
}

void Epd::SendData(unsigned char data) {
    // Framebuffer rendering goes through DisplayFrame instead of SPI bytes.
    (void)data;
}

void Epd::Sleep(void) {
    if (backend_ == nullptr) {
        return;
    }

    // Keep a short desktop test executable alive after its final display call.
    // The real panel retains this exact last image after Sleep() as well.
    while (!backend_->closed) {
        backend_->PumpEvents();
        SDL_Delay(10);
    }
}

void Epd::Clear(void) {
    std::vector<unsigned char> white(EPD_FRAME_BYTES, 0xFF);
    DisplayFrame(white.data());
}
