#pragma once

#include <string>

// Per-platform tricks for actually getting the screen to refresh at the
// target rate. Everything here runs on the game's main (render) thread.
namespace fps_platform {
    // Push the target rate to the OS / display. Safe to call repeatedly.
    void apply(double fps);

    // Highest refresh rate the screen supports, or 0 if unknown.
    double maxRefreshRate();

    // A one-line hint for the player if the game isn't reaching the target
    // (empty string if nothing to say). `measured` is the real FPS.
    std::string blockerHint(double fps, double measured);

    // Whether the cocos director's animation interval should be touched on
    // this platform.
    bool controlsDirectorInterval();
}
