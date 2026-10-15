#include <epd7in5_V2.h>
#include <epd_simulator.h>
#include <array>

int main() {
    Epd display;
    std::array<unsigned char, EPD_FRAME_BYTES> frame;
    frame.fill(0xFF);

    // A display-only pattern, independent of any application or drawing library.
    for (int y = 0; y < EPD_HEIGHT; ++y) {
        for (int x = 0; x < EPD_WIDTH / 8; ++x) {
            if ((x / 10 + y / 80) % 2 == 0) {
                frame[y * (EPD_WIDTH / 8) + x] = 0;
            }
        }
    }

    // Draw once; the runner remains active so the preview can be closed normally.
    bool drawn = false;
    return epd_simulator::Run(display, "7.5-inch e-Paper V2 preview", [&] {
        if (!drawn) {
            display.DisplayFrame(frame.data());
            drawn = true;
        }
    });
}
