#include <Geode/Geode.hpp>
#include "platform.hpp"

#ifdef GEODE_IS_DESKTOP

// On Windows and macOS the game's own frame limiter decides the FPS, so the
// director/application interval set in main.cpp (plus VSync off) is all
// that's needed. Nothing extra to do here.

namespace fps_platform {
    void apply(double) {}
    double maxRefreshRate() { return 0.0; }
    std::string blockerHint(double fps, double measured) {
        if (measured + 5.0 >= fps) return "";
        return fmt::format(
            "Only reaching {:.0f} of {:.0f} FPS. If VSync is on in this mod's settings or your "
            "graphics driver forces it, the game is held to your monitor's refresh rate.",
            measured, fps
        );
    }
    bool controlsDirectorInterval() { return true; }
}

#endif
