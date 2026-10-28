#include <Geode/Geode.hpp>
#include "platform.hpp"

#ifdef GEODE_IS_IOS

#include <objc/runtime.h>
#include <objc/message.h>
#include <cstdlib>
#include <algorithm>

using namespace geode::prelude;

// On iOS, GD draws through Apple's OpenGL ES / GLKit, which Apple caps at
// 60 FPS even on ProMotion (120 Hz) screens. The Geode iOS launcher works
// around this with its "Enable 120hz" option, which swaps in a Metal-backed
// GL layer (ANGLE) and unlocks the display link. When that option is on, the
// launcher sets the ANGLEGLKit environment variable, which is how we detect it.
//
// What this file does:
//  - asks the game's view controller / display link for the target rate,
//  - figures out what the screen can do,
//  - tells the player exactly what to switch on if 120 isn't possible yet.

namespace {
    template <class R = void, class... Args>
    R msg(id obj, const char* sel, Args... args) {
        using Fn = R (*)(id, SEL, Args...);
        return reinterpret_cast<Fn>(objc_msgSend)(obj, sel_registerName(sel), args...);
    }

    id cls(const char* name) {
        return reinterpret_cast<id>(objc_getClass(name));
    }

    bool responds(id obj, const char* sel) {
        return obj && msg<BOOL>(obj, "respondsToSelector:", sel_registerName(sel));
    }

    bool angleActive() {
        const char* v = std::getenv("ANGLEGLKit");
        return v && v[0] == '1';
    }

    // Matches QuartzCore's CAFrameRateRange (iOS 15+)
    struct FrameRateRange {
        float minimum;
        float maximum;
        float preferred;
    };

    void requestOn(id target, double fps) {
        if (!target) return;
        if (responds(target, "setPreferredFrameRateRange:")) {
            FrameRateRange r { static_cast<float>(fps), static_cast<float>(fps), static_cast<float>(fps) };
            msg<void, FrameRateRange>(target, "setPreferredFrameRateRange:", r);
        }
        if (responds(target, "setPreferredFramesPerSecond:")) {
            msg<void, long>(target, "setPreferredFramesPerSecond:", static_cast<long>(fps));
        }
    }
}

namespace fps_platform {
    double maxRefreshRate() {
        id screen = msg<id>(cls("UIScreen"), "mainScreen");
        if (!responds(screen, "maximumFramesPerSecond")) return 60.0;
        return static_cast<double>(msg<long>(screen, "maximumFramesPerSecond"));
    }

    void apply(double fps) {
        double target = std::min(fps, maxRefreshRate());

        // GD's root view controller (GLKViewController-style) drives drawing
        id app = msg<id>(cls("UIApplication"), "sharedApplication");
        id window = app ? msg<id>(app, "keyWindow") : nullptr;
        id root = window ? msg<id>(window, "rootViewController") : nullptr;
        requestOn(root, target);

        // cocos2d-x's own display link, if this build uses one
        if (id callerCls = cls("CCDirectorCaller")) {
            if (responds(callerCls, "sharedDirectorCaller")) {
                id caller = msg<id>(callerCls, "sharedDirectorCaller");
                if (caller) {
                    if (Ivar ivar = class_getInstanceVariable(object_getClass(caller), "displayLink")) {
                        requestOn(object_getIvar(caller, ivar), target);
                    }
                }
            }
        }
    }

    std::string blockerHint(double fps, double measured) {
        if (measured + 5.0 >= fps) return "";
        double max = maxRefreshRate();
        if (max + 1.0 < fps) {
            return fmt::format(
                "This screen tops out at {:.0f} Hz, so that's the most FPS it can show. "
                "120 Hz needs a ProMotion device (iPhone 13 Pro or newer Pro, iPad Pro).", max
            );
        }
        if (!angleActive()) {
            return "Almost there! Open the Geode app, go to Settings, turn on \"Force ANGLE\" "
                   "and then \"Enable 120hz\", and restart. Also make sure Settings > "
                   "Accessibility > Motion > Limit Frame Rate is off.";
        }
        return "";
    }

    // GD on iOS turns the director interval into an integer frame divisor;
    // anything under 1/60 can round to 0. The launcher's 120 Hz mode handles
    // the display link itself, so leave the director alone here.
    bool controlsDirectorInterval() { return false; }
}

#endif
