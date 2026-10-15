#pragma once

#include <functional>

class Epd;

namespace epd_simulator {

// Initialise a preview, then invoke tick on the SDL thread until window close.
// Tick may render with the supplied Epd. This call never invokes Sleep().
int Run(Epd& display, const char* title, const std::function<void()>& tick);

} // namespace epd_simulator
