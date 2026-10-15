# epd7in5_v2_simulator

Native SDL2 emulator for the Waveshare 800 × 480 monochrome e-paper display.
Owns framebuffer preview, physical window sizing and the desktop event loop.
It implements the `Epd` interface for desktop framebuffer display.

Build with a C++17 compiler, CMake and installed SDL2 development files. Set
`SDL2_DIR` or `CMAKE_PREFIX_PATH` if SDL2 is outside the standard package paths.
Dependencies are not downloaded during configuration.

```bash
cmake -S . -B build
cmake --build build -j
./build/display_preview
ctest --test-dir build --output-on-failure
```

The sample displays a generic checkerboard. Tests use SDL's dummy video driver
to check black/white output, partial images, window titles and close events.

Consumers link `epd7in5_v2_simulator` and can call
`epd_simulator::Run(display, title, tick)` to initialize the window and run a
callback on the SDL thread until close. The application supplies its title and
framebuffer. For a one-shot program, `Epd::Sleep()` keeps the preview open until
close; the continuous runner does not call it.

Window sizing defaults are in [`include/epd_simulator_config.h`](include/epd_simulator_config.h).
Edit the `EPD_SIMULATOR_DEFAULT_WIDTH_MM`, `EPD_SIMULATOR_DEFAULT_HEIGHT_MM` and
`EPD_SIMULATOR_DEFAULT_PPI` values and rebuild, or supply them as compiler `-D`
flags. Defaults remain 163.2 × 97.92 mm and 108.9661 PPI (about 700 × 420 pixels).

Runtime settings are read when the window opens:

| Variable | Meaning |
| --- | --- |
| `EPD_SIMULATOR_WIDTH_MM` | Physical preview width in millimetres |
| `EPD_SIMULATOR_HEIGHT_MM` | Physical preview height in millimetres |
| `EPD_SIMULATOR_PPI` | Monitor pixels per inch; overrides detected DPI |

Without a PPI override, detected monitor DPI takes precedence over the default.
Non-numeric, non-finite and non-positive overrides are ignored; dimensions that
cannot produce a supported window size fail initialization. HiDPI scaling is
accounted for; the framebuffer remains 800 × 480 pixels.

```bash
EPD_SIMULATOR_PPI=144 ./build/display_preview
```
