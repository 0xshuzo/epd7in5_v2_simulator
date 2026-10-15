#pragma once

// Window calibration defaults. Edit and rebuild, or override with compiler -D flags.
// Runtime EPD_SIMULATOR_WIDTH_MM, EPD_SIMULATOR_HEIGHT_MM and EPD_SIMULATOR_PPI
// take precedence. Without a PPI override, detected monitor DPI is preferred.
#ifndef EPD_SIMULATOR_DEFAULT_WIDTH_MM
#define EPD_SIMULATOR_DEFAULT_WIDTH_MM 163.2
#endif

#ifndef EPD_SIMULATOR_DEFAULT_HEIGHT_MM
#define EPD_SIMULATOR_DEFAULT_HEIGHT_MM 97.92
#endif

// Used when the monitor cannot report its pixel density.
#ifndef EPD_SIMULATOR_DEFAULT_PPI
#define EPD_SIMULATOR_DEFAULT_PPI 108.96610896610897
#endif
