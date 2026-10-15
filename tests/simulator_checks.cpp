#define SDL_MAIN_HANDLED
#include <SDL.h>
#include <epd7in5_V2.h>
#include <epd_simulator.h>
#include <epd_simulator_config.h>
#include <array>
#include <cmath>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

void Require(bool condition, const char* message) {
    if (!condition) {
        throw std::runtime_error(message);
    }
}
// Sample in panel coordinates even when SDL scales the physical preview window.
static unsigned char ReadPixel(SDL_Renderer* renderer, int x, int y) {
    int width, height;
    SDL_GetRendererOutputSize(renderer, &width, &height);
    std::vector<unsigned char> rgb(width * height * 3);
    Require(
        SDL_RenderReadPixels(renderer, nullptr, SDL_PIXELFORMAT_RGB24, rgb.data(), width * 3) == 0,
        "Cannot read preview pixels");
    return rgb[((y * height / EPD_HEIGHT) * width + x * width / EPD_WIDTH) * 3];
}

int main(int argc, char** argv) {
    try {
        if (argc == 2 && std::string(argv[1]) == "--expect-init-failure") {
            Epd display;
            Require(display.Init() != 0, "Unrepresentable preview dimensions were accepted");
            std::cout << "Unrepresentable preview dimensions rejected\n";
            return 0;
        }
        Require(argc == 1 || argc == 3, "Expected preview width and height");
        const int expected_width = argc == 3
            ? std::stoi(argv[1])
            : static_cast<int>(
                  std::lround(EPD_SIMULATOR_DEFAULT_WIDTH_MM * EPD_SIMULATOR_DEFAULT_PPI / 25.4));
        const int expected_height = argc == 3
            ? std::stoi(argv[2])
            : static_cast<int>(
                  std::lround(EPD_SIMULATOR_DEFAULT_HEIGHT_MM * EPD_SIMULATOR_DEFAULT_PPI / 25.4));

        Epd display;
        int ticks = 0;
        const int result = epd_simulator::Run(display, "Simulator test", [&] {
            Require(++ticks == 1, "Quit event was lost");
            SDL_Window* window = SDL_GetWindowFromID(1);
            Require(window && std::string(SDL_GetWindowTitle(window)) == "Simulator test",
                "Application window title was not applied");

            SDL_Renderer* renderer = SDL_GetRenderer(window);
            Require(renderer != nullptr, "Preview renderer was not created");
            int window_width = 0;
            int window_height = 0;
            SDL_GetWindowSize(window, &window_width, &window_height);
            Require(window_width == expected_width && window_height == expected_height,
                "Preview window has incorrect configured dimensions");
            int output_width = 0;
            int output_height = 0;
            Require(SDL_GetRendererOutputSize(renderer, &output_width, &output_height) == 0 &&
                    output_width == expected_width && output_height == expected_height,
                "Preview output has incorrect configured dimensions");

            // Check both framebuffer bit polarity and the explicit clear operation.
            std::array<unsigned char, EPD_FRAME_BYTES> frame;
            frame.fill(0);
            display.DisplayFrame(frame.data());
            Require(ReadPixel(renderer, 400, 240) == 0, "Black framebuffer displayed incorrectly");
            display.Clear();
            Require(ReadPixel(renderer, 400, 240) == 255, "Clear did not display white");

            // Partial images must leave the rest of the panel white.
            std::array<unsigned char, 100> part{};
            display.Displaypart(part.data(), 80, 80, 80, 10);
            Require(ReadPixel(renderer, 100, 84) == 0 && ReadPixel(renderer, 400, 240) == 255,
                "Partial frame did not preserve white outside the image");

            // A close event must end the loop before the next drawing callback.
            SDL_Event quit{};
            quit.type = SDL_QUIT;
            SDL_PushEvent(&quit);
        });

        Require(result == 0 && ticks == 1, "Simulator runner failed");
        std::cout << "Sizing, framebuffer, partial display, title and event-loop checks passed\n";
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
